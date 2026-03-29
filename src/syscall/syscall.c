#include "syscall.h"
#include "syscall_internal.h"
#include "../gdt/gdt_internal.h"
#include "../vfs/vfs.h"
#include "../process/process.h"
#include "../serial/serial.h"
#include "../scheduler/scheduler.h"
#include "../elf/elf.h"
#include "../pmm/pmm.h"
#include "../vmm/vmm.h"
#include "../vmm/vmm_internal.h"
#include "../gdt/gdt.h"
#include "../lib/string.h"
#include "../heap/heap.h"

/* ── MSR helpers ─────────────────────────────────────────────────── */
static void wrmsr(u32 msr, u64 val) {
    u32 lo = (u32)(val & 0xFFFFFFFFu);
    u32 hi = (u32)(val >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi) : "memory");
}

static u64 rdmsr(u32 msr) {
    u32 lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

/* ── Entry point and globals (syscall_entry.asm) ─────────────────── */
extern void syscall_entry(void);
extern u64  syscall_kernel_rsp;        /* updated by syscall_set_kernel_rsp() */
extern u64  syscall_saved_user_rip;    /* user RIP saved at SYSCALL entry */
extern u64  syscall_saved_user_rsp;    /* user RSP saved at SYSCALL entry */
extern u64  syscall_saved_user_rflags; /* user RFLAGS saved at SYSCALL entry */

/* ── Dispatch table ──────────────────────────────────────────────── */
typedef u64 (*syscall_fn_t)(u64, u64, u64, u64, u64, u64);
static syscall_fn_t dispatch_table[SYSCALL_MAX];

/* ── Handlers ────────────────────────────────────────────────────── */

/* read(fd, buf, count) → bytes read or -EBADF */
static u64 sys_read(u64 fd, u64 buf, u64 count,
                    u64 a3 __attribute__((unused)),
                    u64 a4 __attribute__((unused)),
                    u64 a5 __attribute__((unused))) {
    i32 ret = vfs_fd_read((int)fd, (u32)count, (u8 *)(usize)buf);
    return (u64)(i64)ret;
}

/* write(fd, buf, count) → bytes written or -EBADF */
static u64 sys_write(u64 fd, u64 buf, u64 count,
                     u64 a3 __attribute__((unused)),
                     u64 a4 __attribute__((unused)),
                     u64 a5 __attribute__((unused))) {
    i32 ret = vfs_fd_write((int)fd, (u32)count, (const u8 *)(usize)buf);
    return (u64)(i64)ret;
}

/* open(path, flags, mode) → fd or -EINVAL
   Phase 6 will add path resolution; for now open is a stub. */
static u64 sys_open(u64 a0 __attribute__((unused)),
                    u64 a1 __attribute__((unused)),
                    u64 a2 __attribute__((unused)),
                    u64 a3 __attribute__((unused)),
                    u64 a4 __attribute__((unused)),
                    u64 a5 __attribute__((unused))) {
    return (u64)(i64)(-EINVAL);
}

/* close(fd) */
static u64 sys_close(u64 fd,
                     u64 a1 __attribute__((unused)),
                     u64 a2 __attribute__((unused)),
                     u64 a3 __attribute__((unused)),
                     u64 a4 __attribute__((unused)),
                     u64 a5 __attribute__((unused))) {
    vfs_fd_close((int)fd);
    return 0;
}

/* getpid() → pid */
static u64 sys_getpid(u64 a0 __attribute__((unused)),
                      u64 a1 __attribute__((unused)),
                      u64 a2 __attribute__((unused)),
                      u64 a3 __attribute__((unused)),
                      u64 a4 __attribute__((unused)),
                      u64 a5 __attribute__((unused))) {
    process_t *p = process_current();
    return p ? (u64)p->pid : 0;
}

/* exit(status) — marks current process ZOMBIE and yields */
static u64 sys_exit(u64 status,
                    u64 a1 __attribute__((unused)),
                    u64 a2 __attribute__((unused)),
                    u64 a3 __attribute__((unused)),
                    u64 a4 __attribute__((unused)),
                    u64 a5 __attribute__((unused))) {
    process_t *p = process_current();
    if (p) p->exit_status = (i32)(i64)status;
    process_exit();
    return 0; /* unreachable */
}

/* fork() — COW clone current process; returns child pid in parent, 0 in child */
static u64 sys_fork(u64 a0 __attribute__((unused)),
                    u64 a1 __attribute__((unused)),
                    u64 a2 __attribute__((unused)),
                    u64 a3 __attribute__((unused)),
                    u64 a4 __attribute__((unused)),
                    u64 a5 __attribute__((unused))) {
    i32 ret = process_fork();
    return (u64)(i64)ret;
}

/* ── Embedded ELF table for execve ──────────────────────────────── */
extern const u8  shell_elf_data[];
extern const u32 shell_elf_size;
extern const u8  httpd_elf_data[];
extern const u32 httpd_elf_size;

#define EXEC_USTACK_TOP    0x7FFFF000ULL
#define EXEC_USTACK_PAGES  4
#define EXEC_USER_RFLAGS   0x202ULL
#define EXEC_SEG_USER_CODE 0x23u
#define EXEC_SEG_USER_DATA 0x1Bu

/* execve(path, argv, envp) → does not return on success, -1 on failure */
static u64 sys_execve(u64 path_ptr,
                      u64 a1 __attribute__((unused)),
                      u64 a2 __attribute__((unused)),
                      u64 a3 __attribute__((unused)),
                      u64 a4 __attribute__((unused)),
                      u64 a5 __attribute__((unused))) {
    const char *path = (const char *)(usize)path_ptr;

    /* Find embedded ELF by path */
    const u8 *elf_data = NULL;
    u32       elf_size = 0;
    if (path) {
        const char *p = path, *q = "/bin/shell";
        while (*p && *q && *p == *q) { p++; q++; }
        if (!*p && !*q) {
            elf_data = shell_elf_data;
            elf_size = shell_elf_size;
        } else {
            p = path; q = "/bin/httpd";
            while (*p && *q && *p == *q) { p++; q++; }
            if (!*p && !*q) {
                elf_data = httpd_elf_data;
                elf_size = httpd_elf_size;
            }
        }
    }
    if (!elf_data) return (u64)(i64)(-ENOENT);

    process_t *p = process_current();
    if (!p) return (u64)(i64)(-ESRCH);

    /* Replace address space */
    u64 old_pml4 = p->pml4_phys;
    u64 new_pml4 = vmm_create_user_pml4();
    if (!new_pml4) return (u64)(i64)(-ENOMEM);

    /* Load ELF into new PML4 */
    u64 entry = 0;
    if (elf_load(elf_data, (usize)elf_size, new_pml4, &entry) != 0) {
        vmm_destroy_user_pml4(new_pml4);
        return (u64)(i64)(-ENOEXEC);
    }

    /* Map user stack — zero frames via PHYS_TO_VIRT (higher-half, always accessible) */
    for (u32 i = 0; i < EXEC_USTACK_PAGES; i++) {
        u64 frame = pmm_alloc_frame();
        if (!frame) { vmm_destroy_user_pml4(new_pml4); return (u64)(i64)(-ENOMEM); }
        memset((void *)PHYS_TO_VIRT(frame), 0, PAGE_SIZE);
        u64 vaddr = EXEC_USTACK_TOP - (u64)(EXEC_USTACK_PAGES - i) * PAGE_SIZE;
        vmm_map_user_page(new_pml4, vaddr, frame, PTE_USER | PTE_WRITE);
    }

    /* Switch to new address space and free old */
    p->pml4_phys = new_pml4;
    vmm_switch_to(new_pml4);
    if (old_pml4) vmm_destroy_user_pml4(old_pml4);

    /* Allocate a fresh kernel stack for the new process image.
     * Must be kmalloc'd so it lives in the higher-half (PML4[511], shared
     * with every user PML4) — accessible from ring-0 interrupt handlers
     * regardless of which user PML4 is loaded. */
    u8 *new_kstack = (u8 *)kmalloc(KSTACK_SIZE);
    if (!new_kstack) { vmm_destroy_user_pml4(new_pml4); return (u64)(i64)(-ENOMEM); }
    memset(new_kstack, 0, KSTACK_SIZE);

    u8 *old_kstack = p->kstack;
    p->kstack = new_kstack;

    /* Update TSS RSP0 (interrupts) and SYSCALL kernel RSP */
    u64 new_kstack_top = (u64)new_kstack + KSTACK_SIZE;
    gdt_set_tss_rsp0(new_kstack_top);
    syscall_set_kernel_rsp(new_kstack_top);

    /* Free old kstack AFTER we switch stacks in iretq.
     * We can't free it now since we're still executing on it.
     * Leak it for now — acceptable for a single execve path. */
    (void)old_kstack;

    /* iretq to new userspace entry.  We switch to the new kernel stack
       first so interrupts after iretq use the correct RSP0. */
    u64 usp = EXEC_USTACK_TOP;
    u64 ucs = EXEC_SEG_USER_CODE;
    u64 uss = EXEC_SEG_USER_DATA;
    u64 ufl = EXEC_USER_RFLAGS;
    __asm__ volatile (
        "mov  %0, %%rsp\n\t"      /* switch to new kernel stack top */
        "push %5\n\t"             /* SS  */
        "push %1\n\t"             /* RSP (user stack top) */
        "push %4\n\t"             /* RFLAGS */
        "push %2\n\t"             /* CS  */
        "push %3\n\t"             /* RIP (entry) */
        "iretq\n\t"
        :
        : "r"(new_kstack_top), "r"(usp), "r"(ucs), "r"(entry), "r"(ufl), "r"(uss)
        : "memory"
    );
    __builtin_unreachable();
}

/* waitpid(pid, *status, options) → pid or -1 */
static u64 sys_waitpid(u64 pid, u64 status_ptr,
                       u64 a2 __attribute__((unused)),
                       u64 a3 __attribute__((unused)),
                       u64 a4 __attribute__((unused)),
                       u64 a5 __attribute__((unused))) {
    i32 status = 0;
    i32 ret = process_waitpid((u32)pid, &status);
    if (ret < 0) return (u64)(i64)-1;
    if (status_ptr) *(i32 *)(usize)status_ptr = status;
    return (u64)(u32)ret;
}

/* Stubs for Phase 6 */
static u64 sys_enosys(u64 a0 __attribute__((unused)),
                      u64 a1 __attribute__((unused)),
                      u64 a2 __attribute__((unused)),
                      u64 a3 __attribute__((unused)),
                      u64 a4 __attribute__((unused)),
                      u64 a5 __attribute__((unused))) {
    return (u64)(i64)(-ENOSYS);
}

/* ── syscall_register — called by other modules after init ───────── */
void syscall_register(u32 nr, u64 (*fn)(u64,u64,u64,u64,u64,u64)) {
    if (nr < SYSCALL_MAX)
        dispatch_table[nr] = fn;
}

/* ── syscall_set_kernel_rsp — update SYSCALL entry kernel stack ──── */
void syscall_set_kernel_rsp(u64 rsp) {
    syscall_kernel_rsp = rsp;
}

/* ── syscall_dispatch — called from syscall_entry.asm ────────────── */
u64 syscall_dispatch(u64 nr, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    /* Save user context into current process struct (for fork trampoline). */
    process_save_user_ctx(syscall_saved_user_rip,
                          syscall_saved_user_rsp,
                          syscall_saved_user_rflags);
    if (nr >= SYSCALL_MAX || !dispatch_table[nr]) {
        klog(LOG_WARN, "[syscall] unhandled nr=%u", (unsigned)nr);
        return (u64)(i64)(-ENOSYS);
    }
    return dispatch_table[nr](a1, a2, a3, a4, a5, 0);
}

/* ── syscall_dump ─────────────────────────────────────────────────── */
static void syscall_dump(void) {
    u64 lstar = rdmsr(MSR_LSTAR);
    u64 star  = rdmsr(MSR_STAR);
    klog(LOG_DEBUG, "[syscall] LSTAR=0x%x STAR=0x%x",
         (unsigned)lstar, (unsigned)star);
}

/* ── syscall_init ─────────────────────────────────────────────────── */
int syscall_init(void) {
    u32 i;

    /* Fill table with ENOSYS stubs */
    for (i = 0; i < SYSCALL_MAX; i++)
        dispatch_table[i] = sys_enosys;

    /* Install implemented handlers */
    dispatch_table[SYS_READ]   = sys_read;
    dispatch_table[SYS_WRITE]  = sys_write;
    dispatch_table[SYS_OPEN]   = sys_open;
    dispatch_table[SYS_CLOSE]  = sys_close;
    dispatch_table[SYS_GETPID] = sys_getpid;
    dispatch_table[SYS_EXIT]   = sys_exit;
    dispatch_table[SYS_FORK]   = sys_fork;
    dispatch_table[SYS_EXECVE] = sys_execve;
    dispatch_table[SYS_WAITPID]= sys_waitpid;
    dispatch_table[SYS_MMAP]   = sys_enosys;
    dispatch_table[SYS_MUNMAP] = sys_enosys;
    dispatch_table[SYS_BRK]    = sys_enosys;

    /* Enable SCE (SysCall Enable) in EFER */
    u64 efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | EFER_SCE);

    /* STAR: kernel CS in bits 47:32, user base in bits 63:48
     * SYSCALL loads CS = STAR[47:32]      = SEG_KERNEL_CODE (0x08)
     * SYSRET  loads CS = STAR[63:48]+16|3 = SEG_USER_CODE   (0x23)
     *         loads SS = STAR[63:48]+8 |3 = SEG_USER_DATA   (0x1B)
     * With STAR_USER_BASE = 0x10: SS=0x18|3=0x1B ✓, CS=0x20|3=0x23 ✓  */
    u64 star = ((u64)STAR_USER_BASE  << 48) |
               ((u64)STAR_KERNEL_CS  << 32);
    wrmsr(MSR_STAR,   star);
    wrmsr(MSR_LSTAR,  (u64)(usize)syscall_entry);
    wrmsr(MSR_SFMASK, SFMASK_VALUE);

    klog(LOG_INFO, "[syscall] SYSCALL/SYSRET ready, %u slots", SYSCALL_MAX);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_syscall = {
    .name        = "syscall",
    .initialized = false,
    .init        = syscall_init,
    .dump        = syscall_dump,
    .shutdown    = NULL,
};

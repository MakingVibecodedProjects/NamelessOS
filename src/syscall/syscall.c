#include "syscall.h"
#include "syscall_internal.h"
#include "../gdt/gdt_internal.h"
#include "../vfs/vfs.h"
#include "../process/process.h"
#include "../serial/serial.h"
#include "../scheduler/scheduler.h"

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

/* ── Entry point (defined in syscall_entry.asm) ──────────────────── */
extern void syscall_entry(void);

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
static u64 sys_exit(u64 a0 __attribute__((unused)),
                    u64 a1 __attribute__((unused)),
                    u64 a2 __attribute__((unused)),
                    u64 a3 __attribute__((unused)),
                    u64 a4 __attribute__((unused)),
                    u64 a5 __attribute__((unused))) {
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

/* Stubs for Phase 6 */
static u64 sys_enosys(u64 a0 __attribute__((unused)),
                      u64 a1 __attribute__((unused)),
                      u64 a2 __attribute__((unused)),
                      u64 a3 __attribute__((unused)),
                      u64 a4 __attribute__((unused)),
                      u64 a5 __attribute__((unused))) {
    return (u64)(i64)(-ENOSYS);
}

/* ── syscall_dispatch — called from syscall_entry.asm ────────────── */
u64 syscall_dispatch(u64 nr, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
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
    dispatch_table[SYS_EXECVE] = sys_enosys;
    dispatch_table[SYS_WAITPID]= sys_enosys;
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

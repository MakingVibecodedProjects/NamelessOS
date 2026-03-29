#include "process.h"
#include "process_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"
#include "../heap/heap.h"
#include "../vmm/vmm.h"
#include "../scheduler/scheduler.h"

/* Defined in src/syscall/syscall_entry.asm — SYSRET trampoline for fork child */
extern void fork_sysret_trampoline(void);

/* ── Process table ───────────────────────────────────────────────── */
static process_t  proc_table[PROC_MAX];
static process_t *current_proc = NULL;

/* ── pid allocator ───────────────────────────────────────────────── */
static i32 alloc_pid(void) {
    for (u32 i = 0; i < PROC_MAX; i++) {
        if (proc_table[i].state == PROC_UNUSED)
            return (i32)i;
    }
    return -1;
}

/* ── Public API ──────────────────────────────────────────────────── */

process_t *process_current(void) {
    return current_proc;
}

void process_set_current(process_t *p) {
    current_proc = p;
}

process_t *process_get(u32 pid) {
    if (pid >= PROC_MAX) return NULL;
    if (proc_table[pid].state == PROC_UNUSED) return NULL;
    return &proc_table[pid];
}

i32 kthread_create(const char *name, void (*fn)(void)) {
    i32 pid = alloc_pid();
    if (pid < 0) return -1;

    process_t *p = &proc_table[pid];
    p->pid   = (u32)pid;
    p->state = PROC_READY;
    p->next  = NULL;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';

    /* Allocate kernel stack */
    p->kstack = (u8 *)kmalloc(KSTACK_SIZE);
    if (!p->kstack) {
        p->state = PROC_UNUSED;
        return -1;
    }
    p->pml4_phys = 0;   /* shares kernel address space until fork/exec */

    /* Set up initial context: rip = fn, rsp = stack_top-8.
       context_switch does jmp to ctx.rip with rsp = ctx.rsp directly. */
    u64 stack_top = (u64)(usize)(p->kstack + KSTACK_SIZE);
    p->ctx.rip = (u64)(usize)fn;
    p->ctx.rsp = stack_top - 8;
    p->ctx.rbx = 0;
    p->ctx.rbp = 0;
    p->ctx.r12 = 0;
    p->ctx.r13 = 0;
    p->ctx.r14 = 0;
    p->ctx.r15 = 0;

    klog(LOG_DEBUG, "[process] created thread '%s' pid=%u rip=0x%x",
         name, (unsigned)pid, (unsigned)p->ctx.rip);
    return pid;
}

/* ── process_save_user_ctx ───────────────────────────────────────── */
void process_save_user_ctx(u64 user_rip, u64 user_rsp, u64 user_rflags) {
    if (current_proc) {
        current_proc->user_rip    = user_rip;
        current_proc->user_rsp    = user_rsp;
        current_proc->user_rflags = user_rflags;
    }
}

/* ── process_fork ────────────────────────────────────────────────── */
i32 process_fork(void) {
    if (!current_proc) return -1;

    i32 pid = alloc_pid();
    if (pid < 0) return -1;

    process_t *child = &proc_table[pid];
    child->pid         = (u32)pid;
    child->ppid        = current_proc->pid;
    child->exit_status = 0;
    child->state       = PROC_READY;
    child->next        = NULL;
    strncpy(child->name, current_proc->name, sizeof(child->name) - 1);
    child->name[sizeof(child->name) - 1] = '\0';

    /* Allocate kernel stack */
    child->kstack = (u8 *)kmalloc(KSTACK_SIZE);
    if (!child->kstack) {
        child->state = PROC_UNUSED;
        return -1;
    }

    /* Clone address space (COW) */
    u64 src_pml4 = current_proc->pml4_phys
                   ? current_proc->pml4_phys
                   : vmm_create_user_pml4();   /* promote parent if needed */
    if (!src_pml4) {
        kfree(child->kstack);
        child->state = PROC_UNUSED;
        return -1;
    }
    current_proc->pml4_phys = src_pml4;

    child->pml4_phys = vmm_fork_pml4(src_pml4);
    if (!child->pml4_phys) {
        kfree(child->kstack);
        child->state = PROC_UNUSED;
        return -1;
    }

    /* Copy user-mode context so child can SYSRET back to the same point */
    child->user_rip    = current_proc->user_rip;
    child->user_rsp    = current_proc->user_rsp;
    child->user_rflags = current_proc->user_rflags;

    /* Build child kernel context: land at fork_sysret_trampoline with
       user_rip/rflags/rsp in r12/r13/r14 (callee-saved → restored by
       context_switch).  rsp starts at the top of the fresh kstack. */
    u64 kstack_top = (u64)(usize)(child->kstack + KSTACK_SIZE);
    child->ctx.rip = (u64)(usize)fork_sysret_trampoline;
    child->ctx.rsp = kstack_top - 8;
    child->ctx.rbx = 0;
    child->ctx.rbp = 0;
    child->ctx.r12 = current_proc->user_rip;
    child->ctx.r13 = current_proc->user_rflags;
    child->ctx.r14 = current_proc->user_rsp;
    child->ctx.r15 = 0;

    scheduler_add(child);

    klog(LOG_DEBUG, "[process] forked pid=%u → child pid=%u rip=0x%x rsp=0x%x rflags=0x%x",
         (unsigned)current_proc->pid, (unsigned)pid,
         (unsigned)child->ctx.r12, (unsigned)child->ctx.r14,
         (unsigned)child->ctx.r13);
    return pid;
}

void process_exit(void) {
    if (current_proc)
        current_proc->state = PROC_ZOMBIE;
    /* Yield — scheduler will skip ZOMBIE and pick the next READY process */
    scheduler_yield();
    /* Should not be reached — scheduler won't reschedule a ZOMBIE */
    for (;;) __asm__ volatile ("hlt");
}

/* ── process_waitpid ─────────────────────────────────────────────── */
i32 process_waitpid(u32 pid, i32 *status) {
    /* Busy-wait (spinning yield) until the child becomes ZOMBIE.
       This is a simple implementation — a real kernel would block. */
    process_t *child = NULL;
    if (pid >= PROC_MAX) {
        klog(LOG_DEBUG, "[process] waitpid: pid %u >= PROC_MAX", (unsigned)pid);
        return -1;
    }
    child = &proc_table[pid];
    if (child->state == PROC_UNUSED) {
        klog(LOG_DEBUG, "[process] waitpid: pid %u UNUSED", (unsigned)pid);
        return -1;
    }
    if (child->ppid != current_proc->pid) {
        klog(LOG_DEBUG, "[process] waitpid: pid %u ppid=%u != cur=%u",
             (unsigned)pid, (unsigned)child->ppid,
             current_proc ? (unsigned)current_proc->pid : 99u);
        return -1;
    }

    klog(LOG_DEBUG, "[process] waitpid: waiting for pid %u state=%u", (unsigned)pid, (unsigned)child->state);
    /* Spin until child exits */
    while (child->state != PROC_ZOMBIE)
        scheduler_yield();

    /* Collect exit status and reap — remove from run queue first so the
       slot can be safely reused without the old circular-list node
       remaining linked in the queue. */
    scheduler_remove(child);
    if (status) *status = child->exit_status;
    child->state = PROC_UNUSED;
    return (i32)pid;
}

/* ── process_dump ────────────────────────────────────────────────── */
static void process_dump(void) {
    for (u32 i = 0; i < PROC_MAX; i++) {
        if (proc_table[i].state == PROC_UNUSED) continue;
        static const char *state_names[] = {
            "UNUSED", "READY", "RUNNING", "BLOCKED", "ZOMBIE"
        };
        u8 s = proc_table[i].state;
        klog(LOG_DEBUG, "[process] pid=%u name='%s' state=%s",
             i, proc_table[i].name,
             s < 5 ? state_names[s] : "?");
    }
}

/* ── process_init ────────────────────────────────────────────────── */
int process_init(void) {
    /* Clear table */
    for (u32 i = 0; i < PROC_MAX; i++)
        proc_table[i].state = PROC_UNUSED;

    /* pid 0 = idle.  ctx is zeroed — kernel_main calls scheduler_yield()
       which saves idle's actual boot-stack RSP/RIP into ctx on first switch. */
    process_t *idle = &proc_table[0];
    idle->pid    = 0;
    idle->state  = PROC_RUNNING;
    idle->next   = NULL;
    idle->kstack = NULL;   /* idle runs on the boot stack */
    idle->pml4_phys = 0;
    strncpy(idle->name, "idle", sizeof(idle->name) - 1);

    current_proc = idle;

    klog(LOG_INFO, "[process] process subsystem ready, idle pid=0");
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_process = {
    .name        = "process",
    .initialized = false,
    .init        = process_init,
    .dump        = process_dump,
    .shutdown    = NULL,
};

#include "process.h"
#include "process_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"
#include "../heap/heap.h"
#include "../vmm/vmm.h"
#include "../scheduler/scheduler.h"

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

    /* Set up initial context:
       rip = fn (entry point)
       rsp = top of stack - 8 (16-byte aligned after call push) */
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

/* ── process_fork ────────────────────────────────────────────────── */
i32 process_fork(void) {
    if (!current_proc) return -1;

    i32 pid = alloc_pid();
    if (pid < 0) return -1;

    process_t *child = &proc_table[pid];
    child->pid   = (u32)pid;
    child->state = PROC_READY;
    child->next  = NULL;
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

    /* Copy parent context; child returns 0 from fork */
    child->ctx        = current_proc->ctx;
    child->ctx.rsp    = (u64)(usize)(child->kstack + KSTACK_SIZE) - 8;
    /* rax will be set to 0 by syscall_dispatch returning 0 to child */

    scheduler_add(child);

    klog(LOG_DEBUG, "[process] forked pid=%u → child pid=%u",
         (unsigned)current_proc->pid, (unsigned)pid);
    return pid;
}

void process_exit(void) {
    if (current_proc)
        current_proc->state = PROC_ZOMBIE;
    /* Scheduler will pick the next READY process; for now just halt */
    for (;;) __asm__ volatile ("hlt");
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

    /* pid 0 = idle (current boot context, no kstack allocation needed) */
    process_t *idle = &proc_table[0];
    idle->pid   = 0;
    idle->state = PROC_RUNNING;
    idle->kstack = NULL;   /* boot stack, not kmalloc'd */
    idle->next  = NULL;
    strncpy(idle->name, "idle", sizeof(idle->name) - 1);

    idle->pml4_phys = 0;   /* boot address space */
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

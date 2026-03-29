#include "scheduler.h"
#include "../process/process.h"
#include "../process/process_internal.h"
#include "../timer/timer.h"
#include "../serial/serial.h"
#include "../gdt/gdt.h"
#include "../syscall/syscall.h"

/* ── Tunables ────────────────────────────────────────────────────── */
#define SCHED_TICKS_PER_SLICE   10u   /* yield every 10 ms */

/* ── Run queue (circular singly-linked list via process_t::next) ──── */
static process_t *run_queue = NULL;   /* head of circular list */
static volatile u32 tick_counter = 0;

/* ── Yield-pending flag (checked by irq_common after IRQ dispatch) ── */
volatile u32 scheduler_yield_pending = 0;

/* Declare the assembly context-switch routine */
extern void context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx, u64 new_pml4);

/* ── scheduler_add / run_queue_add ───────────────────────────────── */
void scheduler_add(process_t *p) {
    if (!run_queue) {
        p->next   = p;   /* single-element circular list */
        run_queue = p;
    } else {
        /* Insert after head */
        p->next          = run_queue->next;
        run_queue->next  = p;
    }
}

/* ── scheduler_remove ────────────────────────────────────────────── */
void scheduler_remove(process_t *p) {
    if (!run_queue || !p) return;

    /* Find the node that points to p */
    process_t *prev = run_queue;
    do {
        if (prev->next == p) {
            if (p->next == p) {
                /* p was the only node */
                run_queue = NULL;
            } else {
                prev->next = p->next;
                if (run_queue == p)
                    run_queue = p->next;
            }
            p->next = NULL;
            return;
        }
        prev = prev->next;
    } while (prev != run_queue);
}

/* ── scheduler_tick ──────────────────────────────────────────────── */
/* Called from timer_irq_handler (inside an IRQ handler).  We must NOT
   call scheduler_yield() here because the call chain (timer_irq_handler
   push rbx + call) leaves a misaligned stack frame that corrupts context
   restoration.  Instead, set a flag; irq_common checks it after every
   IRQ dispatch and calls scheduler_yield() from a clean call site. */
void scheduler_tick(void) {
    tick_counter++;
    if (tick_counter >= SCHED_TICKS_PER_SLICE) {
        tick_counter = 0;
        /* idle (pid 0) yields voluntarily in idle_fn — skip the flag to
           avoid attempting preemption through an irq_common iretq frame
           on the boot stack, which has unpredictable depth. */
        process_t *cur = process_current();
        if (cur && cur->pid != 0)
            scheduler_yield_pending = 1;
    }
}

/* ── scheduler_yield ─────────────────────────────────────────────── */
void scheduler_yield(void) {
    if (!run_queue) return;

    process_t *current = process_current();
    process_t *next    = run_queue;

    /* Advance run_queue pointer to find next READY process */
    process_t *start = run_queue;
    do {
        next = next->next;
        if (next->state == PROC_READY || next->state == PROC_RUNNING)
            break;
    } while (next != start);

    if (next == current) return;

    /* Advance the run_queue head so next iteration starts after 'next' */
    run_queue = next;

    /* Mark states */
    if (current && current->state == PROC_RUNNING)
        current->state = PROC_READY;
    next->state = PROC_RUNNING;

    /* Update current_proc via process internals — use process_get(0) trick:
       we call process_current() before the switch, update via the public
       pointer after. The assembly switches stacks; caller's frame is saved. */
    /* Update current process pointer before the switch so the new
       thread sees itself as current when it runs. */
    process_set_current(next);

    /* Update kernel stack pointer for ring-3 → ring-0 transitions */
    if (next->kstack) {
        u64 kstack_top = (u64)(usize)(next->kstack + KSTACK_SIZE);
        gdt_set_tss_rsp0(kstack_top);
        syscall_set_kernel_rsp(kstack_top);
    }

    /* Pass new_pml4 to context_switch so CR3 is written AFTER the stack
       has already been switched to the new process's kernel stack.
       This avoids a window where we hold the old stack while the old PML4
       is gone (boot stack identity mapping is not in user PML4s). */
    cpu_context_t *old_ctx = current ? &current->ctx : &next->ctx;
    context_switch(old_ctx, &next->ctx, next->pml4_phys);
    /* Execution resumes here when THIS process is switched back in.
       The asm barrier prevents GCC from tail-call-optimising the call above
       into a jmp — context_switch reads [rsp] as the return RIP, so it MUST
       be reached via `call`, not `jmp`. */
    __asm__ volatile ("" ::: "memory");
}

/* ── scheduler_init ──────────────────────────────────────────────── */
int scheduler_init(void) {
    /* Add the idle process (pid 0) to the run queue */
    process_t *idle = process_get(0);
    if (!idle) return -1;

    scheduler_add(idle);

    /* Register tick callback with the timer */
    if (timer_register_callback(scheduler_tick) != 0) return -1;

    klog(LOG_INFO, "[scheduler] round-robin scheduler ready");
    return 0;
}

/* ── scheduler_dump ──────────────────────────────────────────────── */
static void scheduler_dump(void) {
    if (!run_queue) {
        klog(LOG_DEBUG, "[scheduler] run queue: empty");
        return;
    }
    process_t *p = run_queue;
    do {
        klog(LOG_DEBUG, "[scheduler] queue pid=%u name='%s'",
             (unsigned)p->pid, p->name);
        p = p->next;
    } while (p != run_queue);
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_scheduler = {
    .name        = "scheduler",
    .initialized = false,
    .init        = scheduler_init,
    .dump        = scheduler_dump,
    .shutdown    = NULL,
};

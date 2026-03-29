#include "scheduler.h"
#include "../process/process.h"
#include "../process/process_internal.h"
#include "../timer/timer.h"
#include "../serial/serial.h"

/* ── Tunables ────────────────────────────────────────────────────── */
#define SCHED_TICKS_PER_SLICE   10u   /* yield every 10 ms */

/* ── Run queue (circular singly-linked list via process_t::next) ──── */
static process_t *run_queue = NULL;   /* head of circular list */
static volatile u32 tick_counter = 0;

/* Declare the assembly context-switch routine */
extern void context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx);

/* ── run_queue_add ───────────────────────────────────────────────── */
static void run_queue_add(process_t *p) {
    if (!run_queue) {
        p->next   = p;   /* single-element circular list */
        run_queue = p;
    } else {
        /* Insert after head */
        p->next          = run_queue->next;
        run_queue->next  = p;
    }
}

/* ── scheduler_tick ──────────────────────────────────────────────── */
void scheduler_tick(void) {
    tick_counter++;
    if (tick_counter >= SCHED_TICKS_PER_SLICE) {
        tick_counter = 0;
        scheduler_yield();
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

    /* If we looped back to the same process, nothing to switch to */
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

    cpu_context_t *old_ctx = current ? &current->ctx : &next->ctx;
    context_switch(old_ctx, &next->ctx);
    /* Execution resumes here when THIS process is switched back in */
}

/* ── scheduler_init ──────────────────────────────────────────────── */
int scheduler_init(void) {
    /* Add the idle process (pid 0) to the run queue */
    process_t *idle = process_get(0);
    if (!idle) return -1;

    run_queue_add(idle);

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

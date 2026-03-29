#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../lib/types.h"
#include "../lib/module.h"
#include "../process/process.h"

/* Initialise the scheduler: add idle to the run queue and register the
   timer tick callback.  Returns 0 on success. */
int  scheduler_init(void);

/* Voluntarily yield the CPU — pick the next READY process and switch. */
void scheduler_yield(void);

/* Called on every IRQ0 tick; increments counter and triggers yield every
   SCHED_TICKS_PER_SLICE ticks. */
void scheduler_tick(void);

/* Add a READY process to the run queue (called by process_fork). */
void scheduler_add(process_t *p);

/* Module descriptor — registered in module_registry after mod_process. */
extern kernel_module_t mod_scheduler;

#endif /* SCHEDULER_H */

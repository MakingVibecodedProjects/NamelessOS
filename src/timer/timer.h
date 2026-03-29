#ifndef TIMER_H
#define TIMER_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Initialise PIT channel 0 at TIMER_HZ (1000 Hz) and register IRQ0.
   Returns 0 on success. */
int  timer_init(void);

/* Return the number of ticks since boot. */
u64  timer_get_ticks(void);

/* Busy-wait for approximately ms milliseconds (uses tick counter). */
void ksleep(u32 ms);

/* Register a callback invoked on every tick (up to TIMER_MAX_CALLBACKS).
   Returns 0 on success, -1 if the table is full. */
int  timer_register_callback(void (*fn)(void));

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_timer;

#endif /* TIMER_H */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "../lib/types.h"

/* ── Spinlock ───────────────────────────────────────────────────────── */
/* A ticket-less test-and-set spinlock.  Good enough for a kernel with
   few CPUs and short critical sections.                                 */

typedef struct {
    volatile u32 locked;
} spinlock_t;

#define SPINLOCK_INIT { .locked = 0 }

/* Acquire: spin until we atomically swap 0 → 1. */
static inline void spin_lock(spinlock_t *l) {
    u32 one = 1, old;
    do {
        __asm__ volatile (
            "xchgl %0, %1"
            : "=r"(old), "+m"(l->locked)
            : "0"(one)
            : "memory"
        );
    } while (old);
}

/* Release: store 0.  The barrier prevents reordering. */
static inline void spin_unlock(spinlock_t *l) {
    __asm__ volatile ("" ::: "memory");
    l->locked = 0;
}

/* Try-acquire: returns 1 if we got the lock, 0 if it was already held. */
static inline int spin_trylock(spinlock_t *l) {
    u32 one = 1, old;
    __asm__ volatile (
        "xchgl %0, %1"
        : "=r"(old), "+m"(l->locked)
        : "0"(one)
        : "memory"
    );
    return (old == 0);
}

#endif /* SPINLOCK_H */

#ifndef PROCESS_INTERNAL_H
#define PROCESS_INTERNAL_H

#include "../lib/types.h"

/* ── Limits ──────────────────────────────────────────────────────── */
#define PROC_MAX        64
#define KSTACK_SIZE     8192ULL   /* 8 KB kernel stack per thread */

/* ── Process states ──────────────────────────────────────────────── */
#define PROC_UNUSED     0
#define PROC_READY      1
#define PROC_RUNNING    2
#define PROC_BLOCKED    3
#define PROC_ZOMBIE     4

/* ── CPU context (callee-saved registers) ────────────────────────── */
/* On a context switch we save/restore only the callee-saved registers
   plus RSP and RIP.  The C ABI guarantees the caller saved the rest. */
typedef struct {
    u64 rbx;
    u64 rbp;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
    u64 rsp;
    u64 rip;
} cpu_context_t;

#endif /* PROCESS_INTERNAL_H */

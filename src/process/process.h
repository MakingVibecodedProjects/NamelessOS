#ifndef PROCESS_H
#define PROCESS_H

#include "../lib/types.h"
#include "../lib/module.h"
#include "process_internal.h"

/* ── Process descriptor ──────────────────────────────────────────── */
typedef struct process {
    u32           pid;
    u8            state;       /* PROC_* constants */
    char          name[32];
    u8           *kstack;      /* base of kernel stack (kmalloc'd) */
    cpu_context_t ctx;         /* saved CPU context */
    struct process *next;      /* intrusive linked list for scheduler */
} process_t;

/* Initialise the process subsystem and create the idle process (pid 0).
   Returns 0 on success. */
int        process_init(void);

/* Create a new kernel thread.  fn is called with no arguments.
   Returns the new pid on success, or -1 on failure. */
i32        kthread_create(const char *name, void (*fn)(void));

/* Mark the current process as ZOMBIE and yield to the scheduler. */
void       process_exit(void);

/* Return a pointer to the currently running process_t. */
process_t *process_current(void);

/* Return a pointer to the process with the given pid, or NULL. */
process_t *process_get(u32 pid);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_process;

#endif /* PROCESS_H */

#ifndef PROCESS_H
#define PROCESS_H

#include "../lib/types.h"
#include "../lib/module.h"
#include "process_internal.h"

/* ── Process descriptor ──────────────────────────────────────────── */
typedef struct process {
    u32           pid;
    u32           ppid;        /* parent PID (0 = no parent / orphan) */
    u8            state;       /* PROC_* constants */
    i32           exit_status; /* exit code set by sys_exit, read by waitpid */
    char          name[32];
    u8           *kstack;      /* base of kernel stack (kmalloc'd) */
    cpu_context_t ctx;         /* saved CPU context */
    u64           pml4_phys;   /* physical address of this process's PML4;
                                  0 = share the boot/kernel address space */
    u64           user_rip;    /* user RIP saved on SYSCALL entry (rcx) */
    u64           user_rsp;    /* user RSP saved on SYSCALL entry */
    u64           user_rflags; /* user RFLAGS saved on SYSCALL entry (r11) */
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

/* COW-fork the current process.  Returns child pid in parent, 0 in child,
   or -1 on failure. */
i32        process_fork(void);

/* Wait for child pid to become ZOMBIE.  Stores exit status via *status.
   Returns pid on success, -1 if no such child or pid not a child. */
i32        process_waitpid(u32 pid, i32 *status);

/* Return a pointer to the currently running process_t. */
process_t *process_current(void);

/* Update the currently running process pointer (called by scheduler). */
void       process_set_current(process_t *p);

/* Return a pointer to the process with the given pid, or NULL. */
process_t *process_get(u32 pid);

/* Save user-mode RIP/RSP/RFLAGS into the current process struct.
   Called from syscall_entry.asm immediately on SYSCALL entry. */
void process_save_user_ctx(u64 user_rip, u64 user_rsp, u64 user_rflags);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_process;

#endif /* PROCESS_H */

#ifndef SYSCALL_INTERNAL_H
#define SYSCALL_INTERNAL_H

#include "../lib/types.h"

/* ── MSR addresses ───────────────────────────────────────────────── */
#define MSR_EFER        0xC0000080u   /* Extended Feature Enable Register     */
#define MSR_STAR        0xC0000081u   /* Syscall target selectors              */
#define MSR_LSTAR       0xC0000082u   /* Syscall target RIP (long mode)        */
#define MSR_SFMASK      0xC0000084u   /* RFLAGS mask applied on SYSCALL        */

#define EFER_SCE        (1u << 0)     /* SYSCALL Enable bit in EFER            */

/* ── RFLAGS bits to mask on SYSCALL entry ────────────────────────── */
/* Clear IF (disable interrupts) and DF on entry; kernel re-enables IF
   explicitly once the kernel stack is set up. */
#define SFMASK_VALUE    (0x200u | 0x400u)   /* IF | DF */

/* ── Linux-compatible syscall numbers ───────────────────────────── */
#define SYS_READ        0
#define SYS_WRITE       1
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_GETPID      39
#define SYS_FORK        57
#define SYS_EXECVE      59
#define SYS_EXIT        60
#define SYS_WAITPID     61
#define SYS_MMAP        9
#define SYS_MUNMAP      11
#define SYS_BRK         12

/* ── Error codes ─────────────────────────────────────────────────── */
#define ENOSYS          38    /* Function not implemented */
#define EBADF           9     /* Bad file descriptor       */
#define EINVAL          22    /* Invalid argument          */
#define ENOENT          2     /* No such file or directory */
#define ENOMEM          12    /* Out of memory             */
#define ESRCH           3     /* No such process           */
#define ENOEXEC         8     /* Exec format error         */

/* ── Dispatch table size ─────────────────────────────────────────── */
#define SYSCALL_MAX     320u  /* covers all numbers used above */

#endif /* SYSCALL_INTERNAL_H */

; syscall.asm — raw SYSCALL wrappers (Linux x86-64 ABI)
;
; Calling convention for userspace syscalls:
;   syscall nr in rax
;   args: rdi rsi rdx r10 r8 r9  (r10 replaces rcx — rcx clobbered by SYSCALL)
;   return value in rax (negative = -errno)
;   rcx and r11 are clobbered by SYSCALL instruction

bits 64

; ── syscall numbers (must match kernel's syscall_internal.h) ────────
%define SYS_READ    0
%define SYS_WRITE   1
%define SYS_OPEN    2
%define SYS_CLOSE   3
%define SYS_BRK     12
%define SYS_GETPID  39
%define SYS_FORK    57
%define SYS_EXECVE  59
%define SYS_EXIT    60
%define SYS_WAITPID 61

; ── Macro: define a 1-arg syscall wrapper ───────────────────────────
; Usage: SYSCALL1 name, nr
%macro SYSCALL1 2
global %1
%1:
    mov  rax, %2
    syscall
    ret
%endmacro

; ── Macro: define a 2-arg syscall wrapper ───────────────────────────
%macro SYSCALL2 2
global %1
%1:
    mov  rax, %2
    syscall
    ret
%endmacro

; ── Macro: define a 3-arg syscall wrapper ───────────────────────────
%macro SYSCALL3 2
global %1
%1:
    mov  rax, %2
    syscall
    ret
%endmacro

; ── Macro: 4-arg (r10 carries 4th arg) ──────────────────────────────
%macro SYSCALL4 2
global %1
%1:
    mov  r10, rcx   ; 4th arg: C passes in rcx, kernel wants r10
    mov  rax, %2
    syscall
    ret
%endmacro

; ── Wrappers ─────────────────────────────────────────────────────────

; ssize_t read(int fd, void *buf, size_t count)
SYSCALL3 read,    SYS_READ

; ssize_t write(int fd, const void *buf, size_t count)
SYSCALL3 write,   SYS_WRITE

; int open(const char *path, int flags, mode_t mode)
SYSCALL3 open,    SYS_OPEN

; int close(int fd)
SYSCALL1 close,   SYS_CLOSE

; void *brk_syscall(void *addr)  — internal; malloc uses this
global brk_syscall
brk_syscall:
    mov  rax, SYS_BRK
    syscall
    ret

; pid_t getpid(void)
SYSCALL1 getpid,  SYS_GETPID

; pid_t fork(void)
global fork
fork:
    mov  rax, SYS_FORK
    syscall
    ret

; int execve(const char *path, char *const argv[], char *const envp[])
SYSCALL3 execve,  SYS_EXECVE

; void _exit(int status)  — noreturn
global _exit
_exit:
    mov  rax, SYS_EXIT
    syscall
    ; kernel marks process ZOMBIE; should not return
.halt:
    hlt
    jmp  .halt

; pid_t waitpid(pid_t pid, int *wstatus, int options)
SYSCALL3 waitpid, SYS_WAITPID

section .note.GNU-stack noalloc noexec nowrite progbits

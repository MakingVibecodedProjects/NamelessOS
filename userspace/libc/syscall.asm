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
%define SYS_MMAP    9
%define SYS_MUNMAP  11
%define SYS_BRK     12
%define SYS_GETPID  39
%define SYS_SOCKET  41
%define SYS_CONNECT 42
%define SYS_ACCEPT  43
%define SYS_SENDTO  44
%define SYS_RECVFROM 45
%define SYS_SHUTDOWN 48
%define SYS_BIND    49
%define SYS_LISTEN  50
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

; ── Macro: 6-arg (r10=4th, r8=5th, r9=6th — already in right regs) ─
%macro SYSCALL6 2
global %1
%1:
    mov  r10, rcx   ; 4th arg: rcx → r10
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

; void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
SYSCALL6 mmap,    SYS_MMAP

; int munmap(void *addr, size_t len)
SYSCALL2 munmap,  SYS_MUNMAP

; void *brk_syscall(void *addr)  — internal; malloc uses this
global brk_syscall
brk_syscall:
    mov  rax, SYS_BRK
    syscall
    ret

; pid_t getpid(void)
SYSCALL1 getpid,  SYS_GETPID

; int socket(int domain, int type, int protocol)
SYSCALL3 socket,  SYS_SOCKET

; int connect(int sockfd, const struct sockaddr_in *addr, uint32_t addrlen)
SYSCALL3 connect, SYS_CONNECT

; int accept(int sockfd, struct sockaddr_in *addr, uint32_t *addrlen)
SYSCALL3 accept,  SYS_ACCEPT

; int send(int sockfd, const void *buf, uint32_t len, int flags)
; Implemented as sendto(fd, buf, len, flags, NULL, 0)
global send
send:
    ; rdi=fd, rsi=buf, rdx=len, rcx=flags — remap to sendto args
    ; sendto: rdi=fd, rsi=buf, rdx=len, r10=flags, r8=0(addr), r9=0(addrlen)
    mov  r10, rcx   ; flags → r10
    xor  r8,  r8    ; addr = NULL
    xor  r9,  r9    ; addrlen = 0
    mov  rax, SYS_SENDTO
    syscall
    ret

; int recv(int sockfd, void *buf, uint32_t len, int flags)
; Implemented as recvfrom(fd, buf, len, flags, NULL, NULL)
global recv
recv:
    ; rdi=fd, rsi=buf, rdx=len, rcx=flags
    mov  r10, rcx   ; flags → r10
    xor  r8,  r8    ; addr = NULL
    xor  r9,  r9    ; addrlen = NULL
    mov  rax, SYS_RECVFROM
    syscall
    ret

; int sendto(int fd, const void *buf, size_t len, int flags,
;            const struct sockaddr_in *addr, uint32_t addrlen)
global sendto
sendto:
    mov  r10, rcx   ; 4th arg: flags → r10
    mov  rax, SYS_SENDTO
    syscall
    ret

; int recvfrom(int fd, void *buf, size_t len, int flags,
;              struct sockaddr_in *addr, uint32_t *addrlen)
global recvfrom
recvfrom:
    mov  r10, rcx   ; 4th arg: flags → r10
    mov  rax, SYS_RECVFROM
    syscall
    ret

; int shutdown(int sockfd, int how)
SYSCALL2 shutdown, SYS_SHUTDOWN

; int bind(int sockfd, const struct sockaddr_in *addr, uint32_t addrlen)
SYSCALL3 bind,    SYS_BIND

; int listen(int sockfd, int backlog)
SYSCALL2 listen,  SYS_LISTEN

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

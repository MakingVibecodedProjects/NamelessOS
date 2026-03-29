; syscall_entry — SYSCALL instruction lands here (LSTAR MSR points here).
;
; On entry (AMD64 ABI for SYSCALL):
;   rax = syscall number
;   rdi, rsi, rdx, r10, r8, r9 = arguments 1-6  (r10 replaces rcx)
;   rcx = saved user RIP  (needed by SYSRET)
;   r11 = saved RFLAGS    (needed by SYSRET)
;   rsp = user stack      (SYSCALL does NOT switch stacks)
;
; Interrupts are disabled by SYSCALL (SFMASK clears IF).
; We switch to the kernel stack immediately.
;
; Kernel stack frame built here:
;   [rsp+0]  user RSP       ← pop rsp restores user stack
;   [rsp+8]  user RIP       ← loaded into rcx for SYSRET
;   [rsp+16] user RFLAGS    ← loaded into r11 for SYSRET
;
; syscall_dispatch(nr, a1, a2, a3, a4, a5) SysV:
;   rdi=nr  rsi=a1  rdx=a2  rcx=a3  r8=a4  r9=a5
;
; process_save_user_ctx() is called from C (syscall_dispatch) using the
; globals syscall_saved_user_rip/rsp/rflags written here before stack switch.
; They are safe because IF=0 until sti, and after cli (before SYSRET) we
; restore rcx/r11 from the kernel stack (not globals) — race-free.

extern syscall_dispatch

section .data

; Kernel RSP for SYSCALL — set by syscall_set_kernel_rsp() whenever
; the current process's kernel stack changes.
global syscall_kernel_rsp
syscall_kernel_rsp: dq 0

; User context saved before stack switch.  Read by syscall_dispatch() via C.
; Written with IF=0; read by dispatch before sti so no concurrent write possible.
global syscall_saved_user_rip
syscall_saved_user_rip:    dq 0

global syscall_saved_user_rsp
syscall_saved_user_rsp:    dq 0

global syscall_saved_user_rflags
syscall_saved_user_rflags: dq 0

section .text

global syscall_entry
syscall_entry:
    ; ── Save user context (IF=0, single CPU) ────────────────────────
    mov [rel syscall_saved_user_rip],    rcx
    mov [rel syscall_saved_user_rsp],    rsp
    mov [rel syscall_saved_user_rflags], r11

    ; ── Switch to kernel stack ───────────────────────────────────────
    mov rsp, [rel syscall_kernel_rsp]

    ; ── Push SYSRET frame on kernel stack ───────────────────────────
    push r11   ; user RFLAGS  → [rsp+16] (after 2 more pushes)
    push rcx   ; user RIP     → [rsp+8]  (after 1 more push)
    push qword [rel syscall_saved_user_rsp]  ; user RSP → [rsp+0]

    ; ── Shuffle register args: syscall_dispatch(nr, a1..a5) ─────────
    ; SysV: rdi=nr, rsi=a1, rdx=a2, rcx=a3, r8=a4, r9=a5
    ; Have: rax=nr, rdi=a1, rsi=a2, rdx=a3, r10=a4, r8=a5
    mov  r9,  r8     ; a5 → r9 first (r9 is scratch here)
    mov  r8,  r10    ; a4 → r8
    mov  rcx, rdx    ; a3 → rcx
    mov  rdx, rsi    ; a2 → rdx
    mov  rsi, rdi    ; a1 → rsi
    mov  rdi, rax    ; nr → rdi

    ; ── Dispatch (interrupts enabled during kernel work) ────────────
    sti
    call syscall_dispatch   ; result in rax
    cli

    ; ── Restore user context from kernel stack ───────────────────────
    ; Stack: [rsp+0]=user_rsp, [rsp+8]=user_rip, [rsp+16]=user_rflags
    ; Load rcx and r11 by offset BEFORE popping rsp (which switches stacks).
    mov  rcx, [rsp+8]    ; user RIP
    mov  r11, [rsp+16]   ; user RFLAGS
    pop  rsp             ; user RSP — switch to user stack

    ; SYSRET64: RIP←rcx, RFLAGS←r11
    o64 sysret

; ── fork_sysret_trampoline ───────────────────────────────────────────────
; Called as the first instruction of a forked child process via context_switch.
; r12/r13/r14 hold the child's user_rip/user_rflags/user_rsp (callee-saved,
; restored by context_switch).
global fork_sysret_trampoline
fork_sysret_trampoline:
    mov  rcx, r12    ; user RIP
    mov  r11, r13    ; user RFLAGS
    mov  rsp, r14    ; user RSP
    xor  eax, eax    ; fork() returns 0 in child
    o64  sysret

; Mark stack as non-executable
section .note.GNU-stack noalloc noexec nowrite progbits

; syscall_entry — SYSCALL instruction lands here (LSTAR MSR points here).
;
; On entry (AMD64 ABI for SYSCALL):
;   rax = syscall number
;   rdi, rsi, rdx, r10, r8, r9 = arguments 1-6  (r10 replaces rcx)
;   rcx = saved user RIP  (do NOT clobber — needed by SYSRET)
;   r11 = saved RFLAGS    (do NOT clobber — needed by SYSRET)
;
; We are still on whatever stack was active when SYSCALL fired.
; For now (no user processes) that is already the kernel stack.
; Phase 6 will add RSP0 swap via TSS.
;
; Calling convention for syscall_dispatch(u64 nr, u64 a1..a5):
;   rdi=nr  rsi=a1  rdx=a2  rcx=a3  r8=a4  r9=a5
; But rcx is already taken by the saved RIP, so we pass a3 via r10→rcx below.

extern syscall_dispatch

global syscall_entry
syscall_entry:
    ; ── Save caller-saved regs that C may clobber ─────────────────
    ; rcx and r11 must survive to SYSRET — push them now.
    push rcx        ; saved user RIP
    push r11        ; saved RFLAGS

    ; The syscall arguments arrive in: rdi rsi rdx r10 r8 r9
    ; syscall_dispatch signature:  (u64 nr, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5)
    ; Map: rdi=rax(nr), rsi=rdi(a1), rdx=rsi(a2), rcx=rdx(a3), r8=r10(a4), r9=r8(a5)
    ; Do the shuffle before clobbering anything.
    mov  r9,  r8     ; a5 = r8  (must move first — r9 is free)
    mov  r8,  r10    ; a4 = r10
    mov  rcx, rdx    ; a3 = rdx
    mov  rdx, rsi    ; a2 = rsi
    mov  rsi, rdi    ; a1 = rdi
    mov  rdi, rax    ; nr = rax

    ; ── Re-enable interrupts now that we are on the kernel stack ──
    sti

    call syscall_dispatch   ; returns result in rax

    ; ── Disable interrupts before SYSRET (RFLAGS restored from r11) ─
    cli

    ; ── Restore saved rcx/r11 for SYSRET ──────────────────────────
    pop  r11
    pop  rcx

    ; SYSRET64: restores RIP from rcx, RFLAGS from r11, CS/SS from STAR
    o64 sysret

; Mark stack as non-executable
section .note.GNU-stack noalloc noexec nowrite progbits

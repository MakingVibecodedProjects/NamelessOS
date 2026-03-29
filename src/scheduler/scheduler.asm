; context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx)
;
; Calling convention (System V AMD64):
;   rdi = old_ctx   (pointer to cpu_context_t to save into)
;   rsi = new_ctx   (pointer to cpu_context_t to restore from)
;
; cpu_context_t layout (must match process_internal.h):
;   +0   rbx
;   +8   rbp
;   +16  r12
;   +24  r13
;   +32  r14
;   +40  r15
;   +48  rsp
;   +56  rip

global context_switch
context_switch:
    ; ── Save callee-saved regs + RSP into *old_ctx ──────────────────
    mov  [rdi +  0], rbx
    mov  [rdi +  8], rbp
    mov  [rdi + 16], r12
    mov  [rdi + 24], r13
    mov  [rdi + 32], r14
    mov  [rdi + 40], r15
    mov  [rdi + 48], rsp

    ; Save return address (the RIP to resume at) — it's on the stack
    ; right now as the return address from the call to context_switch
    mov  rax, [rsp]
    mov  [rdi + 56], rax

    ; ── Restore callee-saved regs + RSP from *new_ctx ───────────────
    mov  rbx, [rsi +  0]
    mov  rbp, [rsi +  8]
    mov  r12, [rsi + 16]
    mov  r13, [rsi + 24]
    mov  r14, [rsi + 32]
    mov  r15, [rsi + 40]
    mov  rsp, [rsi + 48]

    ; Jump to new_ctx->rip
    ; For a freshly created thread rip = fn, rsp = stack_top-8.
    ; The 8-byte gap at stack_top-8 holds a zero (set in kthread_create),
    ; so if the thread function ever returns, it pops 0 and faults cleanly.
    jmp  qword [rsi + 56]

; Mark stack as non-executable (suppresses GNU-stack linker warning)
section .note.GNU-stack noalloc noexec nowrite progbits

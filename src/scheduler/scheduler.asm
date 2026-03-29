; context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx, u64 new_pml4)
;
; Calling convention (System V AMD64):
;   rdi = old_ctx   (pointer to cpu_context_t to save into)
;   rsi = new_ctx   (pointer to cpu_context_t to restore from)
;   rdx = new_pml4  (physical address of new PML4, or 0 to keep current CR3)
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
    ; Save RIP = the return address at [rsp] (pushed by `call context_switch`).
    mov  rax, [rsp]
    mov  [rdi + 56], rax

    ; Save RSP advanced by 8, pointing past the return address.
    ; On restoration, we jmp to ctx.rip (the caller's epilogue first instruction)
    ; with rsp = ctx.rsp.  The epilogue pops its callee-saved registers, then
    ; `ret` pops the return address back to the caller-of-scheduler_yield.
    ; This works regardless of how many registers the compiler saved, because
    ; we skip exactly one slot (the return address) that is already in ctx.rip.
    ; Callers that set ctx.rsp manually (e.g. kthread_create, init_launch) must
    ; subtract 8 from their desired initial RSP to compensate for this +8.
    lea  rax, [rsp + 8]
    mov  [rdi + 48], rax

    ; ── Restore callee-saved regs from *new_ctx ──────────────────────
    mov  rbx, [rsi +  0]
    mov  rbp, [rsi +  8]
    mov  r12, [rsi + 16]
    mov  r13, [rsi + 24]
    mov  r14, [rsi + 32]
    mov  r15, [rsi + 40]

    ; ── Switch stack BEFORE changing CR3 ─────────────────────────────
    ; After this point we are on the new process's kernel stack, which is
    ; always in the higher-half and mapped in every PML4.
    mov  rsp, [rsi + 48]

    ; ── Switch address space (if new_pml4 != 0 and != current CR3) ───
    test rdx, rdx
    jz   .no_cr3
    mov  rax, cr3
    ; Mask out PCID/flags bits before comparing
    mov  rcx, 0xffffffffff000
    and  rax, rcx
    cmp  rax, rdx
    je   .no_cr3
    mov  cr3, rdx
.no_cr3:

    ; Jump to new_ctx->rip.
    jmp  qword [rsi + 56]

; Mark stack as non-executable (suppresses GNU-stack linker warning)
section .note.GNU-stack noalloc noexec nowrite progbits

; NamelessOS src/idt/exceptions.asm
;
; Exception stubs for vectors 0–31.
; Calling convention on entry to exception_dispatch:
;
;   RSP → rsp_at_fault (saved by CPU)
;           rflags
;           cs
;           rip
;           error_code   (0 if CPU didn't push one)
;           vector
;           ← RSP after our push
;
; exception_dispatch (C) signature:
;   void exception_dispatch(u64 vector, u64 error, u64 rip,
;                           u64 cs,    u64 rflags, u64 rsp);

bits 64

; ── C handler (defined in exceptions.c) ──────────────────────────────
extern exception_dispatch

; ── Common trampoline ─────────────────────────────────────────────────
; On entry the stack looks like (top = low address):
;   [rsp+0]  vector
;   [rsp+8]  error_code
;   [rsp+16] rip       ← pushed by CPU
;   [rsp+24] cs
;   [rsp+32] rflags
;   [rsp+40] rsp (ring-0 → same ring, pushed by CPU)
;
; We need to pass these as the first 6 arguments (rdi, rsi, rdx, rcx, r8, r9).
exception_common:
    ; Save scratch registers we'll clobber passing args
    ; (we're about to call a C function, so we need a proper frame;
    ;  we just need RSP 16-byte aligned before the call)
    pop  rdi        ; vector
    pop  rsi        ; error_code
    ; Now stack top = rip (put there by CPU)
    mov  rdx, [rsp]      ; rip
    mov  rcx, [rsp + 8]  ; cs
    mov  r8,  [rsp + 16] ; rflags
    mov  r9,  [rsp + 24] ; rsp at fault

    ; Align stack to 16 bytes (we've done 2 pops = 16 bytes, CPU frame = 5*8 = 40 bytes,
    ; total stack movement from original fault RSP: 40+16 = 56, not aligned → push dummy)
    sub  rsp, 8
    call exception_dispatch
    ; Should not return — PANIC halts. Hang just in case.
.hang:
    cli
    hlt
    jmp .hang

; ── Macro: stub WITHOUT error code (CPU does not push one) ────────────
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0    ; dummy error code
    push qword %1   ; vector number
    jmp  exception_common
%endmacro

; ── Macro: stub WITH error code (CPU already pushed one) ──────────────
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1   ; vector number (error code already on stack below)
    jmp  exception_common
%endmacro

; ── Exception stubs ───────────────────────────────────────────────────
; Ref: Intel SDM Vol 3A Table 6-1
ISR_NOERR  0   ; #DE  Divide Error
ISR_NOERR  1   ; #DB  Debug
ISR_NOERR  2   ;      NMI
ISR_NOERR  3   ; #BP  Breakpoint
ISR_NOERR  4   ; #OF  Overflow
ISR_NOERR  5   ; #BR  Bound Range Exceeded
ISR_NOERR  6   ; #UD  Invalid Opcode
ISR_NOERR  7   ; #NM  Device Not Available
ISR_ERR    8   ; #DF  Double Fault        (error code = 0)
ISR_NOERR  9   ;      Coprocessor Segment Overrun (legacy)
ISR_ERR   10   ; #TS  Invalid TSS
ISR_ERR   11   ; #NP  Segment Not Present
ISR_ERR   12   ; #SS  Stack-Segment Fault
ISR_ERR   13   ; #GP  General Protection Fault
ISR_ERR   14   ; #PF  Page Fault
ISR_NOERR 15   ;      Reserved
ISR_NOERR 16   ; #MF  x87 FPU Error
ISR_ERR   17   ; #AC  Alignment Check
ISR_NOERR 18   ; #MC  Machine Check
ISR_NOERR 19   ; #XM  SIMD FP Exception
ISR_NOERR 20   ; #VE  Virtualization Exception
ISR_ERR   21   ; #CP  Control Protection
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28   ; #HV  Hypervisor Injection
ISR_ERR   29   ; #VC  VMM Communication
ISR_ERR   30   ; #SX  Security Exception
ISR_NOERR 31   ;      Reserved

; Export table so idt.c can iterate — one pointer per vector
section .rodata
global isr_table
isr_table:
    dq isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
    dq isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
    dq isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    dq isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

section .note.GNU-stack noalloc noexec nowrite progbits

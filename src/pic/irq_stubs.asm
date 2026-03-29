; NamelessOS src/pic/irq_stubs.asm
;
; IRQ stubs for vectors 32–47 (IRQ 0–15 after PIC remapping).
; Each stub:
;   1. Pushes the IRQ number (0–15)
;   2. Jumps to irq_common
; irq_common:
;   1. Saves all caller-saved registers
;   2. Calls irq_dispatch(u8 irq)  [irq in rdi]
;   3. Restores registers and iretq

bits 64

extern irq_dispatch

; ── Common IRQ trampoline ─────────────────────────────────────────────
irq_common:
    ; Stack on entry: [rsp+0]=irq_num  then CPU iret frame above
    ; Save scratch registers (System V AMD64 ABI caller-saved)
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; irq number is now at rsp+9*8 = 72 bytes from current rsp
    mov  edi, [rsp + 72]     ; first arg (u8 irq) — zero-extends into edi
    and  edi, 0xFF

    ; Align stack to 16 bytes before C call
    ; Current depth: 9 pushes (72 bytes) + original irq_num push (8) = 80
    ; Plus CPU frame (40 bytes) = 120 — not 16-aligned; sub 8 to align
    sub  rsp, 8
    call irq_dispatch
    add  rsp, 8

    ; Restore scratch registers
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdi
    pop  rsi
    pop  rdx
    pop  rcx
    pop  rax

    add  rsp, 8    ; discard irq_num we pushed in the stub
    iretq

; ── IRQ stub macro ────────────────────────────────────────────────────
%macro IRQ_STUB 1
global irq_stub%1
irq_stub%1:
    push qword %1
    jmp  irq_common
%endmacro

IRQ_STUB  0
IRQ_STUB  1
IRQ_STUB  2
IRQ_STUB  3
IRQ_STUB  4
IRQ_STUB  5
IRQ_STUB  6
IRQ_STUB  7
IRQ_STUB  8
IRQ_STUB  9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15

; ── Stub pointer table (used by pic_init to install gates) ───────────
section .rodata
global irq_stub_table
irq_stub_table:
    dq irq_stub0,  irq_stub1,  irq_stub2,  irq_stub3
    dq irq_stub4,  irq_stub5,  irq_stub6,  irq_stub7
    dq irq_stub8,  irq_stub9,  irq_stub10, irq_stub11
    dq irq_stub12, irq_stub13, irq_stub14, irq_stub15

section .note.GNU-stack noalloc noexec nowrite progbits

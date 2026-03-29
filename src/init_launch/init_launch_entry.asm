; init_iretq_trampoline — entered by context_switch when PID 1 is first scheduled.
;
; At this point:
;   RSP points at the iretq frame built in init_launch.c:
;     [RSP+0]  RIP   (ELF entry point)
;     [RSP+8]  CS    (SEG_USER_CODE = 0x23)
;     [RSP+16] RFLAGS (0x202)
;     [RSP+24] RSP   (user stack top)
;     [RSP+32] SS    (SEG_USER_DATA = 0x1B)
;
; Zero all general-purpose registers (except RSP, set by iretq) before
; entering userspace so init starts with a clean slate.

global init_iretq_trampoline
init_iretq_trampoline:
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    xor rbp, rbp
    iretq

section .note.GNU-stack noalloc noexec nowrite progbits

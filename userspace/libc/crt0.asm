; crt0.asm — C runtime entry point for userspace programs
;
; The kernel jumps here via SYSRET with:
;   rdi = argc
;   rsi = argv  (pointer to array of char* on the user stack)
;   rsp = initial user stack (16-byte aligned)
;
; We call main(argc, argv, NULL) then exit(return value).

bits 64

extern main
extern exit

global _start
_start:
    ; Stack is already aligned to 16 bytes by the kernel.
    ; rdi = argc, rsi = argv — already in the right registers for main.
    xor  rdx, rdx          ; envp = NULL (no environment yet)
    call main
    ; rax = main's return value → exit(rax)
    mov  rdi, rax
    call exit
    ; exit() is noreturn; if it somehow returns, halt.
.halt:
    hlt
    jmp  .halt

section .note.GNU-stack noalloc noexec nowrite progbits

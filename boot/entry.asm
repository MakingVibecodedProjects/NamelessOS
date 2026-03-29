; NamelessOS boot/entry.asm
;
; 1. Multiboot2 header (magic 0xE85250D6)
; 2. 32-bit entry: build page tables, enable PAE + long mode + paging
; 3. Load 64-bit GDT, far-jump to 64-bit code
; 4. 64-bit trampoline: switch to higher-half stack, call kernel_main
;
; Page table strategy:
;   PML4[0]   → PDPT_ID   — identity map first 4 GB (1 GB pages)
;   PML4[511] → PDPT_HH   — higher-half map (mirrors first 4 GB)
;   Both PDPTs map 0–4 GB with 1 GB pages (PDPE_1G).
;
; All page table buffers live in .bss.pagetables which the linker places
; in the low .boot section so their addresses fit in 32-bit registers.

bits 32

; ─────────────────────────────────────────────────────────────────────
; Constants
; ─────────────────────────────────────────────────────────────────────
PDPE_PRESENT  equ 0x1
PDPE_WRITE    equ 0x2
PDPE_1G       equ (1 << 7)           ; PS bit — 1 GB page
CR4_PAE       equ (1 << 5)
CR0_PG        equ (1 << 31)
CR0_PE        equ (1 << 0)
EFER_MSR      equ 0xC0000080
EFER_LME      equ (1 << 8)

; ─────────────────────────────────────────────────────────────────────
; Multiboot2 header — must appear in first 32 KB of file
; ─────────────────────────────────────────────────────────────────────
section .multiboot2
ALIGN 8
mb2_start:
    dd 0xE85250D6                           ; magic
    dd 0                                    ; arch: i386 PM
    dd mb2_end - mb2_start                  ; header length
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_start))  ; checksum
    ; end tag
    dw 0
    dw 0
    dd 8
mb2_end:

; ─────────────────────────────────────────────────────────────────────
; 32-bit protected-mode entry
; ─────────────────────────────────────────────────────────────────────
section .text.boot
global start
extern kernel_main

start:
    cli
    mov  esp, stack32_top           ; tiny 32-bit stack (in .bss.pagetables section)

    ; ── Zero the page table area ──────────────────────────────────
    ; pml4, pdpt_id, pdpt_hh = 3 × 4096 bytes
    mov  edi, pml4
    mov  ecx, (4096 * 3) / 4
    xor  eax, eax
    rep  stosd

    ; ── PML4[0] → pdpt_id  (identity) ────────────────────────────
    mov  eax, pdpt_id
    or   eax, (PDPE_PRESENT | PDPE_WRITE)
    mov  [pml4], eax

    ; ── PML4[511] → pdpt_hh  (higher half) ───────────────────────
    mov  eax, pdpt_hh
    or   eax, (PDPE_PRESENT | PDPE_WRITE)
    mov  [pml4 + 511 * 8], eax

    ; ── Fill pdpt_id[0..3]: 4 × 1 GB identity pages ──────────────
    mov  edi, pdpt_id
    mov  eax, (PDPE_PRESENT | PDPE_WRITE | PDPE_1G)
    mov  ecx, 4
.fill_id:
    mov  [edi], eax
    mov  dword [edi + 4], 0
    add  eax, 0x40000000            ; +1 GB
    add  edi, 8
    loop .fill_id

    ; ── Fill pdpt_hh[510..511]: 2 × 1 GB higher-half pages ───────
    ; 0xFFFFFFFF80000000: PML4[511] PDPT[510] → physical 0x00000000
    ; 0xFFFFFFFFC0000000: PML4[511] PDPT[511] → physical 0x40000000
    mov  edi, pdpt_hh + 510 * 8
    mov  eax, (PDPE_PRESENT | PDPE_WRITE | PDPE_1G | 0x00000000)
    mov  [edi], eax
    mov  dword [edi + 4], 0
    mov  eax, (PDPE_PRESENT | PDPE_WRITE | PDPE_1G | 0x40000000)
    mov  [edi + 8], eax
    mov  dword [edi + 12], 0

    ; ── Enable PAE ────────────────────────────────────────────────
    mov  eax, cr4
    or   eax, CR4_PAE
    mov  cr4, eax

    ; ── Load PML4 into CR3 ────────────────────────────────────────
    mov  eax, pml4
    mov  cr3, eax

    ; ── Set LME in EFER ───────────────────────────────────────────
    mov  ecx, EFER_MSR
    rdmsr
    or   eax, EFER_LME
    wrmsr

    ; ── Enable paging ─────────────────────────────────────────────
    mov  eax, cr0
    or   eax, (CR0_PG | CR0_PE)
    mov  cr0, eax

    ; ── Load 64-bit GDT and far-jump to 64-bit code ───────────────
    lgdt [gdt64_ptr]
    jmp  gdt64.code : long_mode_entry

; ─────────────────────────────────────────────────────────────────────
; 64-bit trampoline  (still at low physical / identity VA)
; ─────────────────────────────────────────────────────────────────────
bits 64
long_mode_entry:
    mov  ax, gdt64.data
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Switch to the higher-half kernel stack
    mov  rsp, stack_top

    ; Absolute call into the higher half
    mov  rax, kernel_main
    call rax

.halt:
    cli
    hlt
    jmp  .halt

; ─────────────────────────────────────────────────────────────────────
; Minimal flat 64-bit GDT
; ─────────────────────────────────────────────────────────────────────
section .text.boot          ; keep in low section alongside entry code
ALIGN 8
gdt64:
    dq 0                    ; null descriptor
.code: equ $ - gdt64
    dq (1<<43)|(1<<44)|(1<<47)|(1<<53)   ; 64-bit code, DPL=0
.data: equ $ - gdt64
    dq (1<<41)|(1<<44)|(1<<47)           ; 64-bit data, DPL=0
gdt64_ptr:
    dw gdt64_ptr - gdt64 - 1
    dq gdt64

; ─────────────────────────────────────────────────────────────────────
; Page table buffers and early stack — LOW physical address required
; nobits tells NASM this is a BSS-style (uninitialized) section
; ─────────────────────────────────────────────────────────────────────
section .bss.pagetables nobits alloc noexec write
ALIGN 4096
pml4:     resb 4096
pdpt_id:  resb 4096
pdpt_hh:  resb 4096

; 32-bit init stack (256 bytes is plenty for the setup code)
ALIGN 16
          resb 256
stack32_top:

; ─────────────────────────────────────────────────────────────────────
; Kernel stack (16 KB) — lives in higher-half .bss
; ─────────────────────────────────────────────────────────────────────
section .bss nobits alloc noexec write
ALIGN 16
stack_bottom: resb 16384
stack_top:

; Mark stack as non-executable (suppresses ld warning)
section .note.GNU-stack noalloc noexec nowrite progbits

#ifndef GDT_INTERNAL_H
#define GDT_INTERNAL_H

#include "../lib/types.h"

/* GDT descriptor indices */
#define GDT_NULL      0
#define GDT_CODE      1   /* kernel code  DPL=0  0x08 */
#define GDT_DATA      2   /* kernel data  DPL=0  0x10 */
#define GDT_USER_DATA 3   /* user data    DPL=3  0x18 — SYSRET SS  */
#define GDT_USER_CODE 4   /* user code    DPL=3  0x20 — SYSRET CS  */
#define GDT_TSS_LO    5   /* TSS low  — two consecutive slots       */
#define GDT_TSS_HI    6   /* TSS high                               */

#define GDT_ENTRY_COUNT 7

/* Segment selector values (index * 8) */
#define SEG_KERNEL_CODE  (GDT_CODE      * 8)        /* 0x08  RPL=0 */
#define SEG_KERNEL_DATA  (GDT_DATA      * 8)        /* 0x10  RPL=0 */
#define SEG_USER_DATA    (GDT_USER_DATA * 8 | 3)    /* 0x1B  RPL=3 */
#define SEG_USER_CODE    (GDT_USER_CODE * 8 | 3)    /* 0x23  RPL=3 */
#define SEG_TSS          (GDT_TSS_LO   * 8)         /* 0x28  RPL=0 */

/* SYSCALL/SYSRET MSR segment layout:
 *   STAR[47:32] = kernel CS (SYSCALL loads CS from here)       = 0x08
 *   STAR[63:48] = base for SYSRET selectors:
 *                 SYSRET SS = base + 8  = 0x18+8  = 0x20 | 3  → but CPU
 *                 uses STAR[63:48]+8 for SS and STAR[63:48]+16 for CS.
 *                 With base=0x18: SS=0x20|3 wrong — we want SS=0x1B.
 *
 * Correct: AMD64 SYSRET64 sets CS = STAR[63:48]+16 | 3,
 *                                SS = STAR[63:48]+8  | 3.
 * We want CS=0x23 (GDT_USER_CODE*8|3) and SS=0x1B (GDT_USER_DATA*8|3).
 *   CS = STAR[63:48]+16 | 3 = 0x23  →  STAR[63:48] = 0x13  → no, must be RPL=0
 *   STAR[63:48] = 0x10: SS=0x18|3=0x1B ✓, CS=0x20|3=0x23 ✓
 */
#define STAR_KERNEL_CS   SEG_KERNEL_CODE   /* 0x08 — SYSCALL CS         */
#define STAR_USER_BASE   SEG_KERNEL_DATA   /* 0x10 — SYSRET base        */

/* ── Raw 64-bit GDT entry ─────────────────────────────────────────
 * We store entries as plain u64 values; all flag manipulation
 * happens in gdt.c via named bit constants. */
typedef u64 gdt_entry_t;

/* ── 64-bit TSS descriptor (16 bytes = two GDT slots) ────────────── */
typedef struct {
    u32 reserved0;
    u64 rsp0;           /* kernel stack for ring-0 (filled in by TSS/IDT later) */
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist[7];         /* Interrupt Stack Table — zeroed for now              */
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;     /* set to sizeof(tss64_t) → no IOPM                    */
} __attribute__((packed)) tss64_t;

/* ── GDTR ──────────────────────────────────────────────────────────── */
typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) gdtr_t;

#endif /* GDT_INTERNAL_H */

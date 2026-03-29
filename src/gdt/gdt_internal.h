#ifndef GDT_INTERNAL_H
#define GDT_INTERNAL_H

#include "../lib/types.h"

/* GDT descriptor indices */
#define GDT_NULL   0
#define GDT_CODE   1
#define GDT_DATA   2
#define GDT_TSS_LO 3   /* TSS occupies two consecutive slots in 64-bit mode */
#define GDT_TSS_HI 4

#define GDT_ENTRY_COUNT 5

/* Segment selector values (index * 8, RPL=0) */
#define SEG_KERNEL_CODE  (GDT_CODE   * 8)
#define SEG_KERNEL_DATA  (GDT_DATA   * 8)
#define SEG_TSS          (GDT_TSS_LO * 8)

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

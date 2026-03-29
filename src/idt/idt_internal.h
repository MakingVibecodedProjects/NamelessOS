#ifndef IDT_INTERNAL_H
#define IDT_INTERNAL_H

#include "../lib/types.h"

#define IDT_ENTRIES 256

/* ── 64-bit interrupt gate descriptor (16 bytes) ─────────────────────
 *
 *  [15:0]   offset[15:0]
 *  [31:16]  segment selector
 *  [34:32]  IST index (0 = no IST)
 *  [39:35]  reserved (0)
 *  [43:40]  type: 0xE = 64-bit interrupt gate
 *  [44]     0 (system descriptor)
 *  [46:45]  DPL
 *  [47]     P (present)
 *  [63:48]  offset[31:16]
 *  [95:64]  offset[63:32]
 *  [127:96] reserved (0)
 */
typedef struct {
    u16 offset_lo;    /* offset[15:0]  */
    u16 selector;     /* code segment  */
    u8  ist;          /* bits[2:0] = IST index, rest zero */
    u8  type_attr;    /* type | DPL | P */
    u16 offset_mid;   /* offset[31:16] */
    u32 offset_hi;    /* offset[63:32] */
    u32 reserved;
} __attribute__((packed)) idt_entry_t;

/* ── IDTR ───────────────────────────────────────────────────────────── */
typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) idtr_t;

/* Type field for a 64-bit present interrupt gate, DPL=0 */
#define IDT_GATE_INTR  0x8E   /* P=1 DPL=0 type=0xE */

/* Kernel code segment selector (must match GDT) */
#define IDT_KERNEL_CS  0x08

#endif /* IDT_INTERNAL_H */

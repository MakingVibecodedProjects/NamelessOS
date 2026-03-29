#ifndef PIC_INTERNAL_H
#define PIC_INTERNAL_H

/* ── 8259A PIC port addresses ────────────────────────────────────── */
#define PIC1_CMD   0x20   /* Master PIC command port  */
#define PIC1_DATA  0x21   /* Master PIC data port     */
#define PIC2_CMD   0xA0   /* Slave  PIC command port  */
#define PIC2_DATA  0xA1   /* Slave  PIC data port     */

/* ── Initialization Command Words ───────────────────────────────── */
#define ICW1_INIT  0x10   /* Start initialisation sequence */
#define ICW1_ICW4  0x01   /* ICW4 needed                   */
#define ICW4_8086  0x01   /* 8086/88 mode                  */

/* ── Operational commands ────────────────────────────────────────── */
#define PIC_EOI    0x20   /* End-of-interrupt command      */
#define PIC_READ_IRR 0x0A /* Read Interrupt Request Register  */
#define PIC_READ_ISR 0x0B /* Read In-Service Register         */

/* ── Vector offsets after remapping ─────────────────────────────── */
#define PIC1_VECTOR_OFFSET 0x20   /* IRQ 0-7  → vectors 32-39  */
#define PIC2_VECTOR_OFFSET 0x28   /* IRQ 8-15 → vectors 40-47  */

/* ── IRQ count ───────────────────────────────────────────────────── */
#define IRQ_COUNT 16

#endif /* PIC_INTERNAL_H */

#ifndef PCI_INTERNAL_H
#define PCI_INTERNAL_H

#include "../lib/types.h"

/* ── PCI config space I/O ports ──────────────────────────────────── */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* ── Config address register layout ─────────────────────────────── */
#define PCI_ADDR_ENABLE     (1UL << 31)
#define PCI_ADDR(bus, slot, func, off) \
    (PCI_ADDR_ENABLE               | \
     ((u32)(bus)  << 16)           | \
     ((u32)(slot) << 11)           | \
     ((u32)(func) <<  8)           | \
     ((u32)(off)  & 0xFC))

/* ── Standard PCI header field byte offsets ──────────────────────── */
#define PCI_OFF_VENDOR      0x00
#define PCI_OFF_DEVICE      0x02
#define PCI_OFF_COMMAND     0x04
#define PCI_OFF_STATUS      0x06
#define PCI_OFF_REVISION    0x08
#define PCI_OFF_PROG_IF     0x09
#define PCI_OFF_SUBCLASS    0x0A
#define PCI_OFF_CLASS       0x0B
#define PCI_OFF_HEADER_TYPE 0x0E
#define PCI_OFF_BAR0        0x10
#define PCI_OFF_BAR1        0x14
#define PCI_OFF_BAR2        0x18
#define PCI_OFF_BAR3        0x1C
#define PCI_OFF_BAR4        0x20
#define PCI_OFF_BAR5        0x24
#define PCI_OFF_INT_LINE    0x3C
#define PCI_OFF_INT_PIN     0x3D

/* ── Header type masks ───────────────────────────────────────────── */
#define PCI_HTYPE_MASK      0x7F
#define PCI_HTYPE_MULTI     0x80   /* multi-function device */

/* ── Enumeration limits ──────────────────────────────────────────── */
#define PCI_MAX_BUS         256
#define PCI_MAX_SLOT        32
#define PCI_MAX_FUNC        8
#define PCI_MAX_DEVICES     64

#endif /* PCI_INTERNAL_H */

#ifndef VMM_INTERNAL_H
#define VMM_INTERNAL_H

#include "../lib/types.h"

/* ── Page / table sizes ──────────────────────────────────────────── */
#define PAGE_SIZE      4096ULL
#define PAGE_MASK      (~(PAGE_SIZE - 1))

/* ── PTE flag bits ───────────────────────────────────────────────── */
#define PTE_PRESENT    (1ULL << 0)
#define PTE_WRITE      (1ULL << 1)
#define PTE_USER       (1ULL << 2)
#define PTE_PWT        (1ULL << 3)
#define PTE_PCD        (1ULL << 4)
#define PTE_ACCESSED   (1ULL << 5)
#define PTE_DIRTY      (1ULL << 6)
#define PTE_HUGE       (1ULL << 7)   /* PS bit — 2MB/1GB page */
#define PTE_GLOBAL     (1ULL << 8)
#define PTE_NX         (1ULL << 63)
#define PTE_COW        (1ULL << 9)    /* OS-defined: copy-on-write pending */

/* Mask to extract the physical frame address from a PTE */
#define PTE_ADDR_MASK  0x000FFFFFFFFFF000ULL

/* ── Virtual address decomposition ──────────────────────────────── */
#define VA_PML4_IDX(va)  (((u64)(va) >> 39) & 0x1FF)
#define VA_PDPT_IDX(va)  (((u64)(va) >> 30) & 0x1FF)
#define VA_PDT_IDX(va)   (((u64)(va) >> 21) & 0x1FF)
#define VA_PT_IDX(va)    (((u64)(va) >> 12) & 0x1FF)

/* ── KERNEL_VMA (must match kernel.ld and entry.asm) ────────────── */
#define KERNEL_VMA     0xFFFFFFFF80000000ULL

/* ── Convert physical ↔ virtual via the higher-half kernel window ────
   pdpt_hh maps 0–2 GB physical → 0xFFFFFFFF80000000–0xFFFFFFFFFFFFFFFF.
   Using KERNEL_VMA + phys keeps kernel pointers in the upper half and
   leaves PML4[0] free for per-process user mappings. */
#define PHYS_TO_VIRT(p)  (KERNEL_VMA + (u64)(p))
#define VIRT_TO_PHYS(v)  ((u64)(v) - KERNEL_VMA)

/* ── Page-table entry types (all are just u64) ───────────────────── */
typedef u64 pml4e_t;
typedef u64 pdpte_t;
typedef u64 pde_t;
typedef u64 pte_t;

/* Number of entries in each level */
#define PT_ENTRIES  512

#endif /* VMM_INTERNAL_H */

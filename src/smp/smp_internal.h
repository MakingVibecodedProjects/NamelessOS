#ifndef SMP_INTERNAL_H
#define SMP_INTERNAL_H

#include "../lib/types.h"

/* ── LAPIC MMIO base ────────────────────────────────────────────────── */
#define LAPIC_BASE          0xFEE00000u

/* LAPIC register offsets (byte offsets from LAPIC_BASE; each register
   is a 32-bit value in a 16-byte-aligned slot). */
#define LAPIC_ID            0x020   /* LAPIC ID register                  */
#define LAPIC_VERSION       0x030   /* LAPIC version                      */
#define LAPIC_TPR           0x080   /* Task Priority Register             */
#define LAPIC_EOI           0x0B0   /* End-Of-Interrupt (write any value) */
#define LAPIC_SVR           0x0F0   /* Spurious Vector Register           */
#define LAPIC_ICR_LO        0x300   /* Interrupt Command Register low 32b */
#define LAPIC_ICR_HI        0x310   /* Interrupt Command Register high    */
#define LAPIC_LVT_TIMER     0x320   /* LVT Timer                          */
#define LAPIC_TIMER_INIT    0x380   /* Timer Initial Count                */
#define LAPIC_TIMER_CUR     0x390   /* Timer Current Count                */
#define LAPIC_TIMER_DIV     0x3E0   /* Timer Divide Configuration         */

/* SVR: enable APIC, spurious vector 0xFF */
#define LAPIC_SVR_ENABLE    (1u << 8)
#define LAPIC_SPURIOUS_VEC  0xFF

/* ICR delivery modes */
#define LAPIC_ICR_FIXED     0x00000000u
#define LAPIC_ICR_INIT      0x00000500u   /* INIT IPI                      */
#define LAPIC_ICR_STARTUP   0x00000600u   /* SIPI                          */
#define LAPIC_ICR_ASSERT    0x00004000u   /* level: assert                 */
#define LAPIC_ICR_DEASSERT  0x00000000u
#define LAPIC_ICR_LEVEL     0x00008000u   /* trigger: level                */
#define LAPIC_ICR_ALLEXSELF 0x000C0000u   /* dest shorthand: all-exc-self  */
#define LAPIC_ICR_PENDING   0x00001000u   /* delivery status bit           */

/* ── LAPIC timer constants ──────────────────────────────────────────── */
#define LAPIC_TIMER_DIV16   0x3             /* divide by 16                */
#define LAPIC_TIMER_VECTOR  0x20            /* same vector as PIT IRQ0 so
                                               existing scheduler_tick()
                                               fires on APs too           */
#define LAPIC_TIMER_PERIODIC (1u << 17)

/* ── IA32_APIC_BASE MSR ─────────────────────────────────────────────── */
#define MSR_IA32_APIC_BASE  0x1B
#define APIC_BASE_GLOBAL_EN (1u << 11)

/* ── ACPI / MP table maximum CPUs ──────────────────────────────────── */
#define SMP_MAX_CPUS        8

/* ── Trampoline physical address ────────────────────────────────────── */
/* Must be below 1 MB and page-aligned.  0x8000 is a safe choice —
   BIOS data ends well below 0x7C00, and our kernel is at 0x100000+. */
#define TRAMPOLINE_PHYS     0x8000u

/* ── GS base MSRs ───────────────────────────────────────────────────── */
#define MSR_GS_BASE         0xC0000101u
#define MSR_KERNEL_GS_BASE  0xC0000102u

/* ── Per-CPU data structure ─────────────────────────────────────────── */
/* Pointed to by GS base on every CPU.  The VERY FIRST field must be a
   self-pointer so that `mov rax, gs:[0]` gives the cpu_t address
   (used by smp_this_cpu() without needing a register-relative addr).  */
typedef struct cpu {
    struct cpu *self;       /* gs:[0]  — self-pointer                    */
    u32         id;         /* logical CPU index (0 = BSP)               */
    u32         lapic_id;   /* hardware LAPIC ID                         */
    u8         *kstack;     /* top of this CPU's kernel stack            */
    void       *current;    /* pointer to current process_t (cast later) */
    u32         online;     /* 1 once AP has finished init               */
} cpu_t;

/* ── Trampoline binary (defined in trampoline_bin.c) ────────────────── */
extern const u8  trampoline_bin[];
extern const u32 trampoline_bin_len;

#endif /* SMP_INTERNAL_H */

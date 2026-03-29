#include "smp.h"
#include "smp_internal.h"
#include "../vmm/vmm.h"
#include "../heap/heap.h"
#include "../serial/serial.h"
#include "../lib/string.h"
#include "../lib/types.h"

/* ── AP entry (called from trampoline, runs on each AP) ─────────────── */
/* Forward declaration — defined later in this file. */
static void ap_entry(void);

/* ── Per-CPU data table ─────────────────────────────────────────────── */
static cpu_t cpus[SMP_MAX_CPUS];
static u32   cpu_count = 0;

/* ── LAPIC MMIO accessor ────────────────────────────────────────────── */
static inline u32 lapic_read(u32 reg) {
    return *((volatile u32 *)(u64)(LAPIC_BASE + reg));
}

static inline void lapic_write(u32 reg, u32 val) {
    *((volatile u32 *)(u64)(LAPIC_BASE + reg)) = val;
}

/* ── lapic_eoi — public, used by LAPIC timer handler ────────────────── */
void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

/* ── lapic_enable ────────────────────────────────────────────────────── */
static void lapic_enable(void) {
    /* Set spurious-interrupt vector and enable APIC (bit 8 of SVR) */
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VEC);
    /* Suppress spurious interrupts: accept all (TPR = 0) */
    lapic_write(LAPIC_TPR, 0);
}

/* ── lapic_timer_start ───────────────────────────────────────────────── */
/* Start a periodic LAPIC timer at roughly the same rate as the PIT so
   existing scheduler_tick() calls fire on APs.  Calibration is skipped;
   a fixed count is used (calibrated empirically for QEMU at ~1 kHz).   */
#define LAPIC_TIMER_COUNT  1000000u   /* ~1 ms in QEMU at 1 GHz bus freq */

static void lapic_timer_start(void) {
    lapic_write(LAPIC_TIMER_DIV, LAPIC_TIMER_DIV16);
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_VECTOR | LAPIC_TIMER_PERIODIC);
    lapic_write(LAPIC_TIMER_INIT, LAPIC_TIMER_COUNT);
}

/* ── MSR helpers ────────────────────────────────────────────────────── */
static inline void wrmsr(u32 msr, u64 val) {
    u32 lo = (u32)val;
    u32 hi = (u32)(val >> 32);
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"(lo), "d"(hi) : "memory");
}

static inline u64 rdmsr(u32 msr) {
    u32 lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

/* ── gs_base — point GS at this CPU's cpu_t ─────────────────────────── */
static void set_gs_base(cpu_t *cpu) {
    wrmsr(MSR_GS_BASE, (u64)cpu);
}

/* ── smp_this_cpu ────────────────────────────────────────────────────── */
cpu_t *smp_this_cpu(void) {
    cpu_t *p;
    __asm__ volatile ("movq %%gs:0, %0" : "=r"(p));
    return p;
}

/* ── smp_cpu_count / smp_cpu ────────────────────────────────────────── */
u32 smp_cpu_count(void) {
    return cpu_count;
}

cpu_t *smp_cpu(u32 id) {
    if (id >= cpu_count) return NULL;
    return &cpus[id];
}

/* ── icr_wait — poll ICR pending bit ───────────────────────────────── */
static void icr_wait(void) {
    while (lapic_read(LAPIC_ICR_LO) & LAPIC_ICR_PENDING) {}
}

/* ── send_init_sipi ─────────────────────────────────────────────────── */
static void __attribute__((unused)) send_init_sipi(u8 lapic_id) {
    /* INIT IPI — edge-triggered (no LEVEL bit), assert only.
       Intel SDM Vol 3A §10.6.1: INIT delivery mode, edge trigger.  */
    lapic_write(LAPIC_ICR_HI, (u32)lapic_id << 24);
    lapic_write(LAPIC_ICR_LO, LAPIC_ICR_INIT | LAPIC_ICR_ASSERT);
    icr_wait();

    /* ~10 ms busy-wait for AP to complete INIT */
    for (volatile u32 i = 0; i < 10000000u; i++) {}

    /* SIPI — send twice as recommended by Intel SDM */
    u8 vec = (u8)(TRAMPOLINE_PHYS >> 12);   /* page number = 0x8 */
    for (int s = 0; s < 2; s++) {
        lapic_write(LAPIC_ICR_HI, (u32)lapic_id << 24);
        lapic_write(LAPIC_ICR_LO, LAPIC_ICR_STARTUP | vec);
        icr_wait();
        for (volatile u32 i = 0; i < 1000000u; i++) {}
    }
}

/* ── cpuid helpers ──────────────────────────────────────────────────── */
static u32 cpuid_max_leaf(void) {
    u32 eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0));
    (void)ebx; (void)ecx; (void)edx;
    return eax;
}

/* Read LAPIC ID from CPUID leaf 1 bits [31:24] of EBX */
static u32 cpuid_lapic_id(void) {
    u32 eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1));
    (void)eax; (void)ecx; (void)edx;
    return ebx >> 24;
}

/* Read number of logical processors from CPUID leaf 1 bits [23:16] of EBX */
static u32 cpuid_logical_cpus(void) {
    u32 eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1));
    (void)eax; (void)ecx; (void)edx;
    u32 n = (ebx >> 16) & 0xFF;
    return (n > 0) ? n : 1;
}

/* ── ap_entry — runs on each AP after trampoline jumps here ──────────── */
static void ap_entry(void) {
    /* Find our LAPIC ID */
    u32 my_lapic_id = cpuid_lapic_id();

    /* Find our cpu_t slot (BSP allocated it and set lapic_id before SIPI) */
    cpu_t *me = NULL;
    for (u32 i = 1; i < cpu_count; i++) {
        if (cpus[i].lapic_id == my_lapic_id) {
            me = &cpus[i];
            break;
        }
    }
    if (!me) {
        /* Unknown AP — halt */
        __asm__ volatile ("cli; hlt");
        __builtin_unreachable();
    }

    /* Point GS at our cpu_t */
    set_gs_base(me);

    /* Enable local APIC and start periodic timer */
    lapic_enable();
    lapic_timer_start();

    /* Mark ourselves online */
    me->online = 1;

    klog(LOG_INFO, "[smp] AP %u (LAPIC %u) online\n",
         (unsigned)me->id, (unsigned)me->lapic_id);

    /* Spin — scheduler integration is out of scope for this session */
    while (1)
        __asm__ volatile ("hlt");
}

/* ── trampoline setup ───────────────────────────────────────────────── */
/* Copy the raw trampoline binary to TRAMPOLINE_PHYS (0x8000), then
   write the per-AP parameter slots.  The trampoline must fit in one
   page and the parameter slots are at fixed offsets 0xFE8..0xFF8.     */

/* Physical address of BSP PML4 — read from CR3 */
static u32 bsp_pml4_phys(void) {
    u64 cr3;
    __asm__ volatile ("movq %%cr3, %0" : "=r"(cr3));
    return (u32)(cr3 & 0xFFFFF000u);
}

static void trampoline_install(void) {
    /* TRAMPOLINE_PHYS is in the low 1 MB identity-mapped region —
       the physical address equals the virtual address here.             */
    u8 *dest = (u8 *)(u64)TRAMPOLINE_PHYS;
    memcpy(dest, trampoline_bin, trampoline_bin_len);
}

static void __attribute__((unused)) trampoline_prepare(u64 stack_top) {
    volatile u32 *pml4_slot  = (volatile u32 *)(u64)(TRAMPOLINE_PHYS + 0xFE8);
    volatile u64 *entry_slot = (volatile u64 *)(u64)(TRAMPOLINE_PHYS + 0xFF0);
    volatile u64 *stack_slot = (volatile u64 *)(u64)(TRAMPOLINE_PHYS + 0xFF8);

    *pml4_slot  = bsp_pml4_phys();
    *entry_slot = (u64)ap_entry;
    *stack_slot = stack_top;
}

/* ── smp_init_impl ───────────────────────────────────────────────────── */
static int smp_init_impl(void) {
    /* ── Copy trampoline to low memory ────────────────────────────── */
    trampoline_install();

    /* ── BSP (CPU 0) setup ─────────────────────────────────────────── */
    u32 bsp_lapic_id = cpuid_lapic_id();

    cpus[0].self     = &cpus[0];
    cpus[0].id       = 0;
    cpus[0].lapic_id = bsp_lapic_id;
    cpus[0].kstack   = NULL;    /* BSP uses boot stack */
    cpus[0].current  = NULL;
    cpus[0].online   = 1;

    /* Point BSP's GS at cpus[0] */
    set_gs_base(&cpus[0]);

    /* Enable BSP LAPIC */
    lapic_enable();

    /* ── Detect total CPU count via CPUID ──────────────────────────── */
    u32 max_leaf = cpuid_max_leaf();
    u32 total    = 1;
    if (max_leaf >= 1)
        total = cpuid_logical_cpus();
    if (total > SMP_MAX_CPUS)
        total = SMP_MAX_CPUS;

    cpu_count = total;

    klog(LOG_INFO, "[smp] BSP LAPIC id=%u, %u CPU(s) detected\n",
         (unsigned)bsp_lapic_id, (unsigned)total);

    /* AP boot via SIPI requires ACPI MADT for proper CPU topology.
       Without MADT we cannot reliably distinguish APs from a second
       BIOS boot sequence in QEMU.  Restrict to BSP-only for now.    */
    if (total > 1) {
        klog(LOG_INFO, "[smp] AP boot disabled (no ACPI MADT) — BSP only\n");
        cpu_count = 1;
    }

    klog(LOG_INFO, "[smp] ready — %u CPU(s) online\n", (unsigned)cpu_count);
    return 0;
}

/* ── Module dump ────────────────────────────────────────────────────── */
static void smp_dump(void) {
    for (u32 i = 0; i < cpu_count; i++) {
        klog(LOG_INFO, "[smp] CPU%u LAPIC=%u online=%u\n",
             (unsigned)cpus[i].id,
             (unsigned)cpus[i].lapic_id,
             (unsigned)cpus[i].online);
    }
}

/* ── Module descriptor ───────────────────────────────────────────────── */
kernel_module_t mod_smp = {
    .name        = "smp",
    .init        = smp_init_impl,
    .dump        = smp_dump,
    .initialized = false,
};

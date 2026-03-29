#include "gdt.h"
#include "gdt_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"

/* ── Descriptor bit constants ────────────────────────────────────── */
/* Access byte bits (byte 5 of an 8-byte descriptor) */
#define DESC_PRESENT   (1ULL << 47)   /* P  — segment is present            */
#define DESC_DPL0      (0ULL << 45)   /* DPL = 0 (kernel)                   */
#define DESC_NON_SYS   (1ULL << 44)   /* S  — code/data (not system)        */
#define DESC_EXEC      (1ULL << 43)   /* E  — executable (code segment)     */
#define DESC_RW        (1ULL << 41)   /* RW — readable code / writable data */
/* Flag nibble bits (byte 6 of an 8-byte descriptor) */
#define DESC_LONG      (1ULL << 53)   /* L  — 64-bit code segment           */
#define DESC_SZ32      (1ULL << 54)   /* D/B — 32-bit (must be 0 if L=1)   */
#define DESC_GRAN      (1ULL << 55)   /* G  — 4KB granularity               */

/* DPL field helpers */
#define DESC_DPL3      (3ULL << 45)   /* DPL = 3 (user) */

/* 64-bit flat code segment: P=1 S=1 E=1 RW=1 L=1 */
#define DESC_KERNEL_CODE \
    (DESC_PRESENT | DESC_DPL0 | DESC_NON_SYS | DESC_EXEC | DESC_RW | DESC_LONG)

/* 64-bit flat data segment: P=1 S=1 RW=1 (L/D/B ignored in 64-bit mode) */
#define DESC_KERNEL_DATA \
    (DESC_PRESENT | DESC_DPL0 | DESC_NON_SYS | DESC_RW)

/* 64-bit user data segment: DPL=3 */
#define DESC_USER_DATA \
    (DESC_PRESENT | DESC_DPL3 | DESC_NON_SYS | DESC_RW)

/* 64-bit user code segment: DPL=3, L=1 */
#define DESC_USER_CODE \
    (DESC_PRESENT | DESC_DPL3 | DESC_NON_SYS | DESC_EXEC | DESC_RW | DESC_LONG)

/* ── TSS descriptor builder ───────────────────────────────────────
 * A 64-bit TSS descriptor is 16 bytes (two consecutive GDT slots).
 * Lower 8 bytes: standard format with type=0x9 (64-bit TSS available).
 * Upper 8 bytes: bits 63:32 of the base address. */
static void gdt_set_tss(gdt_entry_t *lo, gdt_entry_t *hi, u64 base, u32 limit) {
    *lo = 0;
    *lo |= (u64)(limit & 0xFFFF);               /* limit[15:0]           */
    *lo |= (u64)(base  & 0xFFFFFF) << 16;       /* base[23:0]            */
    *lo |= (0x9ULL)                 << 40;       /* type = 64-bit TSS avl */
    *lo |= DESC_PRESENT;                         /* P = 1                 */
    *lo |= (u64)((limit >> 16) & 0xF) << 48;    /* limit[19:16]          */
    *lo |= (u64)((base >> 24) & 0xFF) << 56;    /* base[31:24]           */

    *hi = (u64)(base >> 32);                     /* base[63:32]           */
}

/* ── GDT storage ─────────────────────────────────────────────────── */
static gdt_entry_t gdt[GDT_ENTRY_COUNT];
static tss64_t     tss;
static gdtr_t      gdtr;

/* ── gdt_load: inline asm — lgdt + reload segments ──────────────── */
static void gdt_load(void) {
    gdtr.limit = (u16)(sizeof(gdt) - 1);
    gdtr.base  = (u64)&gdt;

    __asm__ volatile ("lgdt %0" : : "m"(gdtr) : "memory");

    /* Reload CS with a far return: stack = [rip_after_lretq, cs_selector]
       lretq pops RIP first then CS, so push CS first, then RIP. */
    __asm__ volatile (
        "movq  %0, %%rax\n\t"
        "pushq %%rax\n\t"
        "leaq  1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        : : "i"((u64)SEG_KERNEL_CODE) : "rax", "memory"
    );

    /* Reload data/stack segments */
    __asm__ volatile (
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        : : "a"((u16)SEG_KERNEL_DATA) : "memory"
    );

    /* Load TSS */
    __asm__ volatile ("ltr %%ax" : : "a"((u16)SEG_TSS));
}

/* ── gdt_set_tss_rsp0 ─────────────────────────────────────────────── */
void gdt_set_tss_rsp0(u64 rsp0) {
    tss.rsp0 = rsp0;
}

/* ── gdt_dump ─────────────────────────────────────────────────────── */
static void gdt_dump(void) {
    klog(LOG_DEBUG, "[gdt] base=0x%x limit=%u TSS base=0x%x",
         (unsigned int)(u64)&gdt,
         (unsigned int)sizeof(gdt),
         (unsigned int)(u64)&tss);
}

/* ── gdt_init ─────────────────────────────────────────────────────── */
int gdt_init(void) {
    /* Zero everything */
    memset(gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    /* [0] null */
    gdt[GDT_NULL] = 0;

    /* [1] 64-bit kernel code */
    gdt[GDT_CODE] = DESC_KERNEL_CODE;

    /* [2] 64-bit kernel data */
    gdt[GDT_DATA] = DESC_KERNEL_DATA;

    /* [3] 64-bit user data  DPL=3 */
    gdt[GDT_USER_DATA] = DESC_USER_DATA;

    /* [4] 64-bit user code  DPL=3 */
    gdt[GDT_USER_CODE] = DESC_USER_CODE;

    /* [5..6] 64-bit TSS */
    tss.iomap_base = (u16)sizeof(tss64_t);
    gdt_set_tss(&gdt[GDT_TSS_LO], &gdt[GDT_TSS_HI],
                (u64)&tss, (u32)(sizeof(tss64_t) - 1));

    gdt_load();
    klog(LOG_INFO, "[gdt] GDT loaded (%d entries, user segs at 0x%x/0x%x)",
         GDT_ENTRY_COUNT, SEG_USER_DATA, SEG_USER_CODE);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_gdt = {
    .name        = "gdt",
    .initialized = false,
    .init        = gdt_init,
    .dump        = gdt_dump,
    .shutdown    = NULL,
};

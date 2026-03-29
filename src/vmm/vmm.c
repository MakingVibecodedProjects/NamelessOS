#include "vmm.h"
#include "vmm_internal.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../serial/serial.h"
#include "../vga/vga.h"
#include "../pmm/pmm.h"
#include "../idt/idt.h"

/* ── CR3 read/write helpers ──────────────────────────────────────── */
static inline u64 read_cr3(void) {
    u64 val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}
static inline void write_cr3(u64 val) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val) : "memory");
}
static inline u64 read_cr2(void) {
    u64 val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}
static inline void invlpg(u64 virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

/* ── Active PML4 physical address ───────────────────────────────── */
static u64 pml4_phys;

/* ── get_or_alloc_table ──────────────────────────────────────────────
 * Given a pointer to a page-table entry at the current level,
 * return the VA of the next-level table.  If the entry is not present,
 * allocate a fresh frame via PMM, zero it, and install it.
 * Returns NULL on OOM. */
static u64 *get_or_alloc_table(u64 *entry, u64 flags) {
    if (!(*entry & PTE_PRESENT)) {
        u64 frame = pmm_alloc_frame();
        if (!frame) return NULL;
        /* Zero the new table (identity-mapped, so phys == virt) */
        memset((void *)PHYS_TO_VIRT(frame), 0, PAGE_SIZE);
        *entry = frame | flags | PTE_PRESENT;
    }
    return (u64 *)PHYS_TO_VIRT(*entry & PTE_ADDR_MASK);
}

/* ── vmm_map_page ────────────────────────────────────────────────── */
int vmm_map_page(u64 virt, u64 phys, u64 flags) {
    u64 *pml4 = (u64 *)PHYS_TO_VIRT(pml4_phys);

    u64 *pdpt = get_or_alloc_table(&pml4[VA_PML4_IDX(virt)],
                                    PTE_WRITE | PTE_USER);
    if (!pdpt) return -1;

    u64 *pdt  = get_or_alloc_table(&pdpt[VA_PDPT_IDX(virt)],
                                    PTE_WRITE | PTE_USER);
    if (!pdt)  return -1;

    u64 *pt   = get_or_alloc_table(&pdt[VA_PDT_IDX(virt)],
                                    PTE_WRITE | PTE_USER);
    if (!pt)   return -1;

    pt[VA_PT_IDX(virt)] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    invlpg(virt);
    return 0;
}

/* ── vmm_unmap_page ──────────────────────────────────────────────── */
void vmm_unmap_page(u64 virt) {
    u64 *pml4 = (u64 *)PHYS_TO_VIRT(pml4_phys);
    u64  e4   = pml4[VA_PML4_IDX(virt)];
    if (!(e4 & PTE_PRESENT)) return;

    u64 *pdpt = (u64 *)PHYS_TO_VIRT(e4 & PTE_ADDR_MASK);
    u64  e3   = pdpt[VA_PDPT_IDX(virt)];
    if (!(e3 & PTE_PRESENT)) return;

    u64 *pdt  = (u64 *)PHYS_TO_VIRT(e3 & PTE_ADDR_MASK);
    u64  e2   = pdt[VA_PDT_IDX(virt)];
    if (!(e2 & PTE_PRESENT)) return;

    u64 *pt   = (u64 *)PHYS_TO_VIRT(e2 & PTE_ADDR_MASK);
    pt[VA_PT_IDX(virt)] = 0;
    invlpg(virt);
}

/* ── vmm_get_phys ────────────────────────────────────────────────── */
u64 vmm_get_phys(u64 virt) {
    u64 *pml4 = (u64 *)PHYS_TO_VIRT(pml4_phys);
    u64  e4   = pml4[VA_PML4_IDX(virt)];
    if (!(e4 & PTE_PRESENT)) return 0;

    u64 *pdpt = (u64 *)PHYS_TO_VIRT(e4 & PTE_ADDR_MASK);
    u64  e3   = pdpt[VA_PDPT_IDX(virt)];
    if (!(e3 & PTE_PRESENT)) return 0;
    if (e3 & PTE_HUGE)  /* 1 GB page */
        return (e3 & 0x000FFFFFC0000000ULL) | (virt & 0x3FFFFFFFULL);

    u64 *pdt  = (u64 *)PHYS_TO_VIRT(e3 & PTE_ADDR_MASK);
    u64  e2   = pdt[VA_PDT_IDX(virt)];
    if (!(e2 & PTE_PRESENT)) return 0;
    if (e2 & PTE_HUGE)  /* 2 MB page */
        return (e2 & 0x000FFFFFFFE00000ULL) | (virt & 0x1FFFFFULL);

    u64 *pt   = (u64 *)PHYS_TO_VIRT(e2 & PTE_ADDR_MASK);
    u64  e1   = pt[VA_PT_IDX(virt)];
    if (!(e1 & PTE_PRESENT)) return 0;
    return (e1 & PTE_ADDR_MASK) | (virt & (PAGE_SIZE - 1));
}

/* ── Page fault handler ──────────────────────────────────────────── */
/* Non-static so pf_stub (naked asm) can reference it by name. */
void page_fault_handler_c(u64 err, u64 rip,
                           u64 cs,  u64 rflags, u64 rsp) {
    u64 cr2 = read_cr2();
    kprintf_set_output(serial_putchar);
    kprintf("\n*** PAGE FAULT ***\n");
    kprintf("  CR2 (fault addr) : 0x%x\n",  (unsigned int)cr2);
    kprintf("  Error code       : 0x%x",     (unsigned int)err);
    kprintf(" [%s][%s][%s]\n",
            (err & 1) ? "PROT"  : "NP",
            (err & 2) ? "WRITE" : "READ",
            (err & 4) ? "USER"  : "KERN");
    kprintf("  RIP              : 0x%x\n",  (unsigned int)rip);
    kprintf("  CS               : 0x%x\n",  (unsigned int)cs);
    kprintf("  RFLAGS           : 0x%x\n",  (unsigned int)rflags);
    kprintf("  RSP              : 0x%x\n",  (unsigned int)rsp);
    (void)cs; (void)rflags; (void)rsp;
    for (;;) __asm__ volatile ("cli; hlt");
}

/* Raw ISR stub for vector 14 (#PF).  CPU pushes: error, rip, cs, rflags, rsp.
   We need to call page_fault_handler_c(err, rip, cs, rflags, rsp). */
__attribute__((naked))
static void pf_stub(void) {
    __asm__ volatile (
        /* Stack on entry (top → bottom):
           [rsp+0]  error_code   ← pushed by CPU
           [rsp+8]  rip
           [rsp+16] cs
           [rsp+24] rflags
           [rsp+32] rsp_at_fault */
        "mov  0(%rsp),  %rdi\n\t"   /* err    */
        "mov  8(%rsp),  %rsi\n\t"   /* rip    */
        "mov  16(%rsp), %rdx\n\t"   /* cs     */
        "mov  24(%rsp), %rcx\n\t"   /* rflags */
        "mov  32(%rsp), %r8\n\t"    /* rsp    */
        "sub  $8,       %rsp\n\t"   /* align  */
        "call page_fault_handler_c\n\t"
        "add  $8,       %rsp\n\t"
        "iretq\n\t"
    );
}

/* ── vmm_dump ────────────────────────────────────────────────────── */
static void vmm_dump(void) {
    klog(LOG_DEBUG, "[vmm] CR3=0x%x", (unsigned int)read_cr3());
}

/* ── vmm_init ────────────────────────────────────────────────────── */
int vmm_init(void) {
    /* Grab physical address of the boot PML4 from CR3 */
    pml4_phys = read_cr3() & PTE_ADDR_MASK;

    /* Install our own page fault handler (replaces the generic one) */
    idt_set_gate(14, (void *)pf_stub, 0);

    klog(LOG_INFO, "[vmm] VMM ready, CR3=0x%x", (unsigned int)pml4_phys);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_vmm = {
    .name        = "vmm",
    .initialized = false,
    .init        = vmm_init,
    .dump        = vmm_dump,
    .shutdown    = NULL,
};

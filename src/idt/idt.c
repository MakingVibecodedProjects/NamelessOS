#include "idt.h"
#include "idt_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"

/* ── ISR table from exceptions.asm ──────────────────────────────── */
extern void *isr_table[32];

/* ── IDT storage ─────────────────────────────────────────────────── */
static idt_entry_t idt[IDT_ENTRIES];
static idtr_t      idtr;

/* ── idt_set_gate ────────────────────────────────────────────────── */
void idt_set_gate(u8 vec, void *handler, u8 ist) {
    u64 addr = (u64)handler;
    idt[vec].offset_lo  = (u16)(addr & 0xFFFF);
    idt[vec].selector   = IDT_KERNEL_CS;
    idt[vec].ist        = ist & 0x7;
    idt[vec].type_attr  = IDT_GATE_INTR;
    idt[vec].offset_mid = (u16)((addr >> 16) & 0xFFFF);
    idt[vec].offset_hi  = (u32)(addr >> 32);
    idt[vec].reserved   = 0;
}

/* ── idt_dump ────────────────────────────────────────────────────── */
static void idt_dump(void) {
    klog(LOG_DEBUG, "[idt] base=0x%x limit=%u",
         (unsigned int)(u64)idt, (unsigned int)sizeof(idt));
}

/* ── idt_init ────────────────────────────────────────────────────── */
int idt_init(void) {
    memset(idt, 0, sizeof(idt));

    /* Install exception stubs for vectors 0–31 */
    for (int i = 0; i < 32; i++)
        idt_set_gate((u8)i, isr_table[i], 0);

    idtr.limit = (u16)(sizeof(idt) - 1);
    idtr.base  = (u64)idt;

    __asm__ volatile ("lidt %0" : : "m"(idtr) : "memory");

    klog(LOG_INFO, "[idt] IDT loaded (%d entries)", IDT_ENTRIES);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_idt = {
    .name        = "idt",
    .initialized = false,
    .init        = idt_init,
    .dump        = idt_dump,
    .shutdown    = NULL,
};

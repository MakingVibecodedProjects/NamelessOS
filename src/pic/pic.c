#include "pic.h"
#include "pic_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"
#include "../idt/idt.h"

/* ── I/O helpers ─────────────────────────────────────────────────── */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
/* Short I/O delay — write to port 0x80 (POST debug port, harmless) */
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"((u8)0));
}

/* ── IRQ handler table ───────────────────────────────────────────── */
static void (*irq_handlers[IRQ_COUNT])(void);

/* ── IRQ stubs declared in irq_stubs.asm ────────────────────────── */
extern void *irq_stub_table[IRQ_COUNT];

/* ── pic_eoi ─────────────────────────────────────────────────────── */
void pic_eoi(u8 irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* ── irq_enable ──────────────────────────────────────────────────── */
void irq_enable(u8 irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  bit  = irq & 7;
    outb(port, inb(port) & (u8)~(1u << bit));
}

/* ── irq_disable ─────────────────────────────────────────────────── */
void irq_disable(u8 irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  bit  = irq & 7;
    outb(port, inb(port) | (u8)(1u << bit));
}

/* ── irq_register ────────────────────────────────────────────────── */
void irq_register(u8 irq, void (*handler)(void)) {
    if (irq < IRQ_COUNT)
        irq_handlers[irq] = handler;
}

/* ── irq_dispatch — called from irq_stubs.asm ───────────────────── */
void irq_dispatch(u8 irq) {
    if (irq < IRQ_COUNT && irq_handlers[irq])
        irq_handlers[irq]();
    pic_eoi(irq);
}

/* ── pic_dump ────────────────────────────────────────────────────── */
static void pic_dump(void) {
    u8 mask1 = inb(PIC1_DATA);
    u8 mask2 = inb(PIC2_DATA);
    klog(LOG_DEBUG, "[pic] IMR master=0x%x slave=0x%x",
         (unsigned int)mask1, (unsigned int)mask2);
}

/* ── pic_init ────────────────────────────────────────────────────── */
int pic_init(void) {
    memset(irq_handlers, 0, sizeof(irq_handlers));

    /* Save current masks */
    u8 mask1 = inb(PIC1_DATA);
    u8 mask2 = inb(PIC2_DATA);

    /* ICW1: start initialisation */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();

    /* ICW2: vector offsets */
    outb(PIC1_DATA, PIC1_VECTOR_OFFSET); io_wait();
    outb(PIC2_DATA, PIC2_VECTOR_OFFSET); io_wait();

    /* ICW3: master has slave on IRQ2; slave ID = 2 */
    outb(PIC1_DATA, 0x04); io_wait();   /* bit 2 = IRQ2 has slave */
    outb(PIC2_DATA, 0x02); io_wait();   /* slave cascaded via IRQ2 */

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Restore masks — then immediately mask everything */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
    outb(PIC1_DATA, 0xFF);   /* mask all master IRQs */
    outb(PIC2_DATA, 0xFF);   /* mask all slave IRQs  */

    /* Install IRQ stubs into IDT at vectors 32–47 */
    for (int i = 0; i < IRQ_COUNT; i++)
        idt_set_gate((u8)(PIC1_VECTOR_OFFSET + i), irq_stub_table[i], 0);

    klog(LOG_INFO, "[pic] PIC remapped, IRQs 0-15 -> vectors 32-47");
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_pic = {
    .name        = "pic",
    .initialized = false,
    .init        = pic_init,
    .dump        = pic_dump,
    .shutdown    = NULL,
};

#include "serial.h"
#include "../lib/printf.h"

/* ── COM1 port constants ─────────────────────────────────────────── */
#define COM1_BASE       0x3F8
#define COM1_DATA       (COM1_BASE + 0)
#define COM1_IER        (COM1_BASE + 1)
#define COM1_BAUD_LO    (COM1_BASE + 0)   /* DLAB=1 */
#define COM1_BAUD_HI    (COM1_BASE + 1)   /* DLAB=1 */
#define COM1_FCR        (COM1_BASE + 2)
#define COM1_LCR        (COM1_BASE + 3)
#define COM1_MCR        (COM1_BASE + 4)
#define COM1_LSR        (COM1_BASE + 5)

#define LCR_8N1         0x03
#define LCR_DLAB        0x80
#define MCR_DTR_RTS     0x03
#define LSR_THR_EMPTY   0x20
#define BAUD_115200_DIV 1

/* ── I/O port helpers ────────────────────────────────────────────── */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* ── serial_init ─────────────────────────────────────────────────── */
int serial_init(void) {
    outb(COM1_IER,     0x00);
    outb(COM1_LCR,     LCR_DLAB);
    outb(COM1_BAUD_LO, BAUD_115200_DIV);
    outb(COM1_BAUD_HI, 0x00);
    outb(COM1_LCR,     LCR_8N1);
    outb(COM1_FCR,     0xC7);
    outb(COM1_MCR,     MCR_DTR_RTS);
    klog(LOG_INFO, "[serial] COM1 initialized at 115200 baud");
    return 0;
}

/* ── serial_putchar ──────────────────────────────────────────────── */
void serial_putchar(char c) {
    while (!(inb(COM1_LSR) & LSR_THR_EMPTY));
    outb(COM1_DATA, (u8)c);
}

/* ── serial_write ────────────────────────────────────────────────── */
void serial_write(const char *s) {
    while (*s) serial_putchar(*s++);
}

/* ── klog ────────────────────────────────────────────────────────── */
static const char *const level_str[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "PANIC"
};

void klog(log_level_t level, const char *fmt, ...) {
    kprintf_set_output(serial_putchar);
    kprintf("[%s] ", level_str[level]);

    typedef __builtin_va_list va_list;
    va_list ap;
    __builtin_va_start(ap, fmt);
    vkprintf(fmt, ap);
    __builtin_va_end(ap);

    serial_putchar('\n');
}

/* ── serial_dump ─────────────────────────────────────────────────── */
void serial_dump(void) {
    klog(LOG_DEBUG, "[serial] COM1 @ 0x3F8, 115200 baud, 8N1");
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_serial = {
    .name        = "serial",
    .initialized = false,
    .init        = serial_init,
    .dump        = serial_dump,
    .shutdown    = NULL,
};

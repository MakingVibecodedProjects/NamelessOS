#ifndef PANIC_H
#define PANIC_H

#include "../lib/printf.h"
#include "../serial/serial.h"
#include "../vga/vga.h"

/* PANIC — prints file:line:function + message to both serial and VGA, then halts. */
#define PANIC(fmt, ...) do {                                            \
    kprintf_set_output(serial_putchar);                                 \
    kprintf("\n*** KERNEL PANIC ***\n");                                \
    kprintf("  File : %s\n", __FILE__);                                 \
    kprintf("  Line : %d\n", __LINE__);                                 \
    kprintf("  Func : %s\n", __func__);                                 \
    kprintf("  Msg  : " fmt "\n", ##__VA_ARGS__);                       \
    kprintf_set_output(vga_putchar);                                    \
    kprintf("\n*** KERNEL PANIC ***\n");                                \
    kprintf("  File : %s\n", __FILE__);                                 \
    kprintf("  Line : %d\n", __LINE__);                                 \
    kprintf("  Func : %s\n", __func__);                                 \
    kprintf("  Msg  : " fmt "\n", ##__VA_ARGS__);                       \
    for (;;) __asm__ volatile ("cli; hlt");                             \
} while (0)

#endif /* PANIC_H */

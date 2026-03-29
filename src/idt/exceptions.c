#include "../lib/types.h"
#include "../lib/printf.h"
#include "../serial/serial.h"
#include "../vga/vga.h"
#include "../core/panic.h"

/* Exception names for vectors 0–31 */
static const char *const exc_names[32] = {
    "Divide Error",              /*  0 #DE */
    "Debug",                     /*  1 #DB */
    "NMI",                       /*  2     */
    "Breakpoint",                /*  3 #BP */
    "Overflow",                  /*  4 #OF */
    "Bound Range Exceeded",      /*  5 #BR */
    "Invalid Opcode",            /*  6 #UD */
    "Device Not Available",      /*  7 #NM */
    "Double Fault",              /*  8 #DF */
    "Coprocessor Segment",       /*  9     */
    "Invalid TSS",               /* 10 #TS */
    "Segment Not Present",       /* 11 #NP */
    "Stack-Segment Fault",       /* 12 #SS */
    "General Protection Fault",  /* 13 #GP */
    "Page Fault",                /* 14 #PF */
    "Reserved",                  /* 15     */
    "x87 FP Exception",          /* 16 #MF */
    "Alignment Check",           /* 17 #AC */
    "Machine Check",             /* 18 #MC */
    "SIMD FP Exception",         /* 19 #XM */
    "Virtualization Exception",  /* 20 #VE */
    "Control Protection",        /* 21 #CP */
    "Reserved",                  /* 22     */
    "Reserved",                  /* 23     */
    "Reserved",                  /* 24     */
    "Reserved",                  /* 25     */
    "Reserved",                  /* 26     */
    "Reserved",                  /* 27     */
    "Hypervisor Injection",      /* 28 #HV */
    "VMM Communication",         /* 29 #VC */
    "Security Exception",        /* 30 #SX */
    "Reserved",                  /* 31     */
};

/* Called from the common exception trampoline in exceptions.asm.
   Arguments arrive in the order the stub left them on the stack
   and we moved to registers before calling. */
void exception_dispatch(u64 vector, u64 error, u64 rip,
                        u64 cs,     u64 rflags, u64 rsp) {
    const char *name = (vector < 32) ? exc_names[vector] : "Unknown";

    /* Log to serial first (VGA may be broken depending on the fault) */
    kprintf_set_output(serial_putchar);
    kprintf("\n*** CPU EXCEPTION ***\n");
    kprintf("  Vec    : %u (%s)\n", (unsigned int)vector, name);
    kprintf("  Error  : 0x%x\n",   (unsigned int)error);
    kprintf("  RIP    : 0x%x\n",   (unsigned int)rip);
    kprintf("  CS     : 0x%x\n",   (unsigned int)cs);
    kprintf("  RFLAGS : 0x%x\n",   (unsigned int)rflags);
    kprintf("  RSP    : 0x%x\n",   (unsigned int)rsp);

    /* Then VGA */
    kprintf_set_output(vga_putchar);
    kprintf("\n*** CPU EXCEPTION: %s ***\n", name);
    kprintf("  Vec=%u Err=0x%x RIP=0x%x\n",
            (unsigned int)vector, (unsigned int)error, (unsigned int)rip);

    for (;;) __asm__ volatile ("cli; hlt");
}

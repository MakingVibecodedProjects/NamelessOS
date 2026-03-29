#ifndef PIC_H
#define PIC_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Remap PIC and mask all IRQs. Returns 0 on success. */
int  pic_init(void);

/* Register a handler for IRQ line irq (0–15). */
void irq_register(u8 irq, void (*handler)(void));

/* Unmask (enable) an IRQ line. */
void irq_enable(u8 irq);

/* Mask (disable) an IRQ line. */
void irq_disable(u8 irq);

/* Send End-Of-Interrupt for the given IRQ line. */
void pic_eoi(u8 irq);

/* Called from irq_stubs.asm — dispatches to the registered handler. */
void irq_dispatch(u8 irq);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_pic;

#endif /* PIC_H */

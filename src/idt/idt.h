#ifndef IDT_H
#define IDT_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Initialize the IDT: install exception stubs for vectors 0-31,
   fill remaining 256 entries with a default handler, call lidt.
   Returns 0 on success. */
int  idt_init(void);

/* Install a single interrupt gate.
   handler: address of the raw asm stub.
   ist: IST stack index (0 = none). */
void idt_set_gate(u8 vec, void *handler, u8 ist);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_idt;

#endif /* IDT_H */

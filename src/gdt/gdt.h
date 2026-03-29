#ifndef GDT_H
#define GDT_H

#include "../lib/module.h"

/* Initialize the GDT (null/code/data/TSS descriptors) and reload
   all segment registers.  Returns 0 on success. */
int gdt_init(void);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_gdt;

#endif /* GDT_H */

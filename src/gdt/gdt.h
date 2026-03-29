#ifndef GDT_H
#define GDT_H

#include "../lib/module.h"

/* Initialize the GDT (null/code/data/TSS descriptors) and reload
   all segment registers.  Returns 0 on success. */
int gdt_init(void);

/* Update TSS.RSP0 — the kernel stack pointer loaded on ring-3→ring-0
   transitions (syscall, interrupts from userspace).  Call this before
   switching to a user process. */
void gdt_set_tss_rsp0(u64 rsp0);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_gdt;

#endif /* GDT_H */

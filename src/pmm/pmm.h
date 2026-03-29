#ifndef PMM_H
#define PMM_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Store the Multiboot2 info pointer before modules_init_all() is called. */
void pmm_set_mb2(u64 mb2_info_phys);

/* Parse Multiboot2 memory map and initialise the bitmap allocator.
   Returns 0 on success. */
int  pmm_init(void);

/* Allocate one 4 KB physical frame.  Returns physical address, or 0 on OOM. */
u64  pmm_alloc_frame(void);

/* Free a previously allocated 4 KB physical frame. */
void pmm_free_frame(u64 phys);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_pmm;

#endif /* PMM_H */

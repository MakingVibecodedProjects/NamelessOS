#ifndef VMM_H
#define VMM_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Standard kernel mapping flags */
#define VMM_KERN_RW  (PTE_PRESENT | PTE_WRITE)
#define VMM_KERN_RO  (PTE_PRESENT)

/* Avoid pulling in vmm_internal from callers — re-export the flags they need */
#define PTE_PRESENT    (1ULL << 0)
#define PTE_WRITE      (1ULL << 1)
#define PTE_USER       (1ULL << 2)
#define PTE_NX         (1ULL << 63)

/* Take ownership of the boot PML4 and set up the VMM.  Returns 0 on success. */
int  vmm_init(void);

/* Map one 4KB page: virt (page-aligned VA) → phys (page-aligned PA) with flags. */
int  vmm_map_page(u64 virt, u64 phys, u64 flags);

/* Unmap a single 4KB page; fires invlpg. */
void vmm_unmap_page(u64 virt);

/* Walk page tables and return the physical address for virt, or 0 if unmapped. */
u64  vmm_get_phys(u64 virt);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_vmm;

#endif /* VMM_H */

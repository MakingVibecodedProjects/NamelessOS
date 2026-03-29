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

/* ── Per-process address space ───────────────────────────────────── */

/* Allocate a new PML4 whose upper 256 entries (kernel half) are copied
   from the boot PML4.  User half is zeroed.  Returns physical address. */
u64  vmm_create_user_pml4(void);

/* Map a 4 KB page into a specific address space (identified by pml4_phys). */
int  vmm_map_user_page(u64 pml4_phys, u64 virt, u64 phys, u64 flags);

/* COW clone: copy user half of src_pml4 into a new PML4, marking all
   writable user PTEs as read-only + PTE_COW in both src and dst.
   Returns the new PML4 physical address, or 0 on OOM. */
u64  vmm_fork_pml4(u64 src_pml4_phys);

/* Free all user-space page table frames (not the mapped pages themselves)
   and the PML4 frame.  Kernel half entries are not freed. */
void vmm_destroy_user_pml4(u64 pml4_phys);

/* Map a kernel (ring-0 only) page into a user process's PML4 at the given
   virtual address.  No PTE_USER is set on any level — the page is invisible
   to user code but accessible to the kernel (e.g. for kernel stacks).
   Used to make kstacks accessible when CR3 is the user PML4. */
int  vmm_map_kernel_in_user_pml4(u64 pml4_phys, u64 virt, u64 phys, u64 flags);

/* Load a PML4 into CR3 (flush TLB). */
void vmm_switch_to(u64 pml4_phys);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_vmm;

#endif /* VMM_H */

#ifndef ELF_H
#define ELF_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Load an ELF64 executable from a byte buffer into the address space
   identified by pml4_phys.
   - buf      : pointer to the raw ELF image in kernel memory
   - buf_size : size of the buffer in bytes
   - pml4_phys: target process's PML4 physical address
   - entry_out: receives the virtual entry point on success
   Returns 0 on success, negative on failure. */
int elf_load(const u8 *buf, usize buf_size,
             u64 pml4_phys, u64 *entry_out);

/* Module descriptor — no init work needed; registered for dump only. */
extern kernel_module_t mod_elf;

#endif /* ELF_H */

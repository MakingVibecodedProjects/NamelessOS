#ifndef HEAP_H
#define HEAP_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Initialise the kernel heap.  Must be called after VMM is ready.
   Returns 0 on success. */
int   heap_init(void);

/* Allocate at least `size` bytes of kernel memory.
   Returns a kernel-VA pointer, or NULL on failure.
   The returned block is NOT zeroed. */
void *kmalloc(usize size);

/* Allocate `size` bytes zeroed to 0. */
void *kzalloc(usize size);

/* Resize a previous kmalloc allocation.
   If ptr is NULL, behaves like kmalloc(size).
   If size is 0, frees ptr and returns NULL. */
void *krealloc(void *ptr, usize size);

/* Free a block returned by kmalloc/kzalloc/krealloc. */
void  kfree(void *ptr);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_heap;

#endif /* HEAP_H */

#ifndef DYNLINK_H
#define DYNLINK_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Kernel module — installs sys_mmap and sys_munmap.
   Provides anonymous page-mapped memory regions for userspace
   (used by malloc, future dynamic loader, etc.). */

extern kernel_module_t mod_dynlink;

#endif /* DYNLINK_H */

#ifndef DEVFS_H
#define DEVFS_H

#include "../lib/types.h"
#include "../lib/module.h"
#include "../vfs/vfs.h"

/* Initialise devfs: create /dev in tmpfs root, register built-in devices.
   Returns 0 on success. */
int devfs_init(void);

/* Register a new device node under /dev with the given name, type flags,
   and custom ops vtable.  Returns 0 on success, -1 on error. */
int devfs_register(const char *name, u32 flags, fs_ops_t *ops);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_devfs;

#endif /* DEVFS_H */

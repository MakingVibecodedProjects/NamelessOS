#ifndef TMPFS_H
#define TMPFS_H

#include "../lib/types.h"
#include "../lib/module.h"
#include "../vfs/vfs.h"

/* Initialise tmpfs, register it with VFS, and mount on "/".
   Returns 0 on success. */
int tmpfs_init(void);

/* Create a file or directory node under parent dir.
   flags: VFS_FILE or VFS_DIR.  Returns the new inode or NULL on error. */
struct tmpfs_inode *tmpfs_create(vfs_node_t *parent, const char *name, u32 flags);

/* Fill a vfs_node_t from a tmpfs_inode (needed by devfs to install custom ops). */
void tmpfs_inode_to_node(struct tmpfs_inode *in, vfs_node_t *out);

/* filesystem_t backend — pass to vfs_register_fs if mounting manually. */
extern filesystem_t tmpfs_fs;

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_tmpfs;

#endif /* TMPFS_H */

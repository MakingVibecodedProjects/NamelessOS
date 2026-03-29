#ifndef VFS_H
#define VFS_H

#include "../lib/types.h"
#include "../lib/module.h"
#include "vfs_internal.h"

/* Forward declarations */
struct vfs_node;
struct fs_ops;

/* ── File system operations vtable ──────────────────────────────── */
typedef struct fs_ops {
    /* Read up to size bytes at offset into buf.  Returns bytes read or -1. */
    i32  (*read)   (struct vfs_node *node, u32 offset, u32 size, u8 *buf);
    /* Write up to size bytes at offset from buf.  Returns bytes written or -1. */
    i32  (*write)  (struct vfs_node *node, u32 offset, u32 size, const u8 *buf);
    /* Open: called when a node is opened.  Returns 0 on success. */
    int  (*open)   (struct vfs_node *node);
    /* Close: called when fd is released. */
    void (*close)  (struct vfs_node *node);
    /* Read directory entry index into out_node.  Returns 0 on success, -1 at end. */
    int  (*readdir)(struct vfs_node *node, u32 index, struct vfs_node *out_node);
    /* Find a child named name.  Returns 0 and fills out_node on success. */
    int  (*finddir)(struct vfs_node *node, const char *name, struct vfs_node *out_node);
} fs_ops_t;

/* ── VFS node ────────────────────────────────────────────────────── */
typedef struct vfs_node {
    char      name[VFS_NAME_MAX];
    u32       flags;      /* VFS_FILE | VFS_DIR | ... */
    u32       inode;      /* filesystem-specific identifier */
    u32       size;       /* file size in bytes */
    u32       uid;
    u32       gid;
    u32       mask;       /* permissions */
    fs_ops_t *ops;        /* vtable — NULL if not mounted */
    struct vfs_node *ptr; /* used for mountpoints and symlinks */
} vfs_node_t;

/* ── Filesystem backend ──────────────────────────────────────────── */
typedef struct {
    char name[32];
    /* Mount: given a device node, return the root vfs_node or NULL on error. */
    vfs_node_t *(*mount)(vfs_node_t *device);
} filesystem_t;

/* ── VFS lifecycle ───────────────────────────────────────────────── */

/* Initialise VFS subsystem.  Returns 0 on success. */
int  vfs_init(void);

/* Register a filesystem backend (e.g. fat32, ext2, tmpfs). */
int  vfs_register_fs(filesystem_t *fs);

/* Mount a registered filesystem by name onto path using device node dev.
   Pass NULL for dev if the filesystem needs no device (e.g. tmpfs). */
int  vfs_mount(const char *fs_name, const char *path, vfs_node_t *dev);

/* ── Node operations (call through vtable) ───────────────────────── */

/* Read up to size bytes at offset into buf.  Returns bytes read, or -1. */
i32  vfs_read   (vfs_node_t *node, u32 offset, u32 size, u8 *buf);

/* Write up to size bytes at offset from buf.  Returns bytes written, or -1. */
i32  vfs_write  (vfs_node_t *node, u32 offset, u32 size, const u8 *buf);

/* Open a node (increments internal ref).  Returns 0 on success. */
int  vfs_open   (vfs_node_t *node);

/* Close a node. */
void vfs_close  (vfs_node_t *node);

/* Read directory entry at index into out_node.  Returns 0, or -1 at end. */
int  vfs_readdir(vfs_node_t *node, u32 index, vfs_node_t *out_node);

/* Find child named name.  Returns 0 and fills out_node on success. */
int  vfs_finddir(vfs_node_t *node, const char *name, vfs_node_t *out_node);

/* ── File-descriptor table ───────────────────────────────────────── */

/* Open a node and allocate a file descriptor.  Returns fd >= 0, or -1. */
int  vfs_fd_open   (vfs_node_t *node);

/* Read from fd.  Returns bytes read, or -1. */
i32  vfs_fd_read   (int fd, u32 size, u8 *buf);

/* Write to fd.  Returns bytes written, or -1. */
i32  vfs_fd_write  (int fd, u32 size, const u8 *buf);

/* Close fd. */
void vfs_fd_close  (int fd);

/* Expose the VFS root node (set by vfs_mount on path "/"). */
extern vfs_node_t *vfs_root;

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_vfs;

#endif /* VFS_H */

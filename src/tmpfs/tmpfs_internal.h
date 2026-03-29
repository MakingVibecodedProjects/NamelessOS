#ifndef TMPFS_INTERNAL_H
#define TMPFS_INTERNAL_H

#include "../lib/types.h"

/* ── Limits ──────────────────────────────────────────────────────── */
#define TMPFS_MAX_NODES     64
#define TMPFS_MAX_CHILDREN  16     /* max entries per directory */
#define TMPFS_MAX_FILE_SIZE 65536  /* 64 KB per file */

/* ── Internal inode ──────────────────────────────────────────────── */
typedef struct tmpfs_inode {
    char   name[128];
    u32    flags;          /* VFS_FILE | VFS_DIR */
    u8    *data;           /* file data (kmalloc'd); NULL for dirs */
    u32    size;           /* current data size in bytes */
    struct tmpfs_inode *children[TMPFS_MAX_CHILDREN];
    u32    child_count;
    bool   used;
} tmpfs_inode_t;

#endif /* TMPFS_INTERNAL_H */

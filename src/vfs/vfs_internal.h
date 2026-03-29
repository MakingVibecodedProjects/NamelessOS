#ifndef VFS_INTERNAL_H
#define VFS_INTERNAL_H

/* ── Limits ──────────────────────────────────────────────────────── */
#define VFS_MAX_FS      8     /* registered filesystem backends */
#define VFS_MAX_FDS     64    /* open file descriptors */
#define VFS_NAME_MAX    128   /* max filename length (incl. NUL) */
#define VFS_PATH_MAX    256   /* max path length (incl. NUL) */

/* ── Node type flags ─────────────────────────────────────────────── */
#define VFS_FILE        0x01
#define VFS_DIR         0x02
#define VFS_CHARDEV     0x04
#define VFS_BLOCKDEV    0x08
#define VFS_SYMLINK     0x10
#define VFS_MOUNTPOINT  0x20

#endif /* VFS_INTERNAL_H */

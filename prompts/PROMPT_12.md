# PROMPT_12 — Phase 4 Step 3: tmpfs

**Session date:** 2026-03-29
**Status when starting:** Phase 4 Step 2 complete (VFS abstract layer, fs_ops vtable, fd table)
**Status when done:** Phase 4 Step 3 complete — tmpfs mounted on /, VFS root set

## What was built

- `src/tmpfs/tmpfs_internal.h` — `TMPFS_MAX_NODES=64`, `TMPFS_MAX_CHILDREN=16`, `TMPFS_MAX_FILE_SIZE=65536`; `tmpfs_inode_t` (name, flags, data ptr, size, children[], child_count, used)
- `src/tmpfs/tmpfs.h` — `tmpfs_init()`, `filesystem_t tmpfs_fs`, `mod_tmpfs`
- `src/tmpfs/tmpfs.c` — static inode pool; `fs_ops_t` vtable (read/write with krealloc resize, open/close no-op, readdir/finddir by linear scan); `tmpfs_mount` allocates root dir inode; `tmpfs_create` helper (unused, kept for devfs/future use); registers with VFS and mounts on "/"

## Key decisions

- **Static inode pool** — avoids fragmentation; 64 nodes is sufficient for a tmpfs root hierarchy
- **`krealloc` on write** — file data grows on demand up to 64 KB; gaps zeroed with `memset`
- **`__attribute__((unused))` on `tmpfs_create`** — helper needed by upcoming devfs/initrd but not called from tmpfs itself; avoids `-Wunused-function`
- **`inode` field = pool index** — `node_to_inode` recovers the inode in O(1) without a hash map

## Verified serial output

```
[DEBUG] [vfs] registered filesystem: tmpfs
[INFO] [vfs] mounted tmpfs on /
[INFO] [tmpfs] mounted on /
```

## Next session prompt

Implement **Phase 4 Step 4**: `src/devfs/` — device filesystem mounted on `/dev`.

- `src/devfs/devfs.h` — `devfs_init()`, `devfs_register(const char *name, u32 flags, fs_ops_t *ops)`, `mod_devfs`
- `src/devfs/devfs.c`:
  - Uses tmpfs as backing store — creates a `/dev` directory in the tmpfs root, then creates child nodes for each registered device
  - Built-in devices registered on init:
    - `/dev/null` — read returns 0, write discards
    - `/dev/zero` — read fills buf with 0x00
  - `devfs_register()` creates a new node under `/dev` with the given ops
  - On init: create `/dev` dir in tmpfs root, then register null/zero
  - Log `[devfs] /dev ready`
- Register `mod_devfs` after `mod_tmpfs` in module_registry
- Zero warnings policy applies

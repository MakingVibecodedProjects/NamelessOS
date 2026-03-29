[← 11](PROMPT_11.md) | [index](README.md) | **12** | [13 →](PROMPT_13.md)

---

# PROMPT_12 — Phase 4 Step 3: tmpfs

**Session date:** 2026-03-29
**Status when starting:** Phase 4 Step 2 complete (VFS abstract layer, fs_ops vtable, fd table)
**Status when done:** Phase 4 Step 3 complete — tmpfs mounted on /, VFS root set

## What was built

- `src/tmpfs/tmpfs_internal.h` — `TMPFS_MAX_NODES=64`, `TMPFS_MAX_CHILDREN=16`, `TMPFS_MAX_FILE_SIZE=65536`; `tmpfs_inode_t` (name, flags, data ptr, size, children[], child_count, used)
- `src/tmpfs/tmpfs.h` — `tmpfs_init()`, `filesystem_t tmpfs_fs`, `mod_tmpfs`
- `src/tmpfs/tmpfs.c` — static inode pool; `fs_ops_t` vtable (read/write with krealloc resize, open/close no-op, readdir/finddir by linear scan); `tmpfs_mount` allocates root dir inode; registers with VFS and mounts on "/"

## Key decisions

- **Static inode pool** — avoids fragmentation; 64 nodes is sufficient for a tmpfs root hierarchy
- **`krealloc` on write** — file data grows on demand up to 64 KB; gaps zeroed with `memset`
- **`__attribute__((unused))` on `tmpfs_create`** — helper needed by devfs but not called from tmpfs itself; avoids `-Wunused-function`
- **`inode` field = pool index** — O(1) inode lookup without a hash map

## Verified serial output

```
[DEBUG] [vfs] registered filesystem: tmpfs
[INFO] [vfs] mounted tmpfs on /
[INFO] [tmpfs] mounted on /
```

## Next session

[PROMPT_13 →](PROMPT_13.md) — devfs: /dev/null, /dev/zero, devfs_register extensible API.

---

[← 11](PROMPT_11.md) | [index](README.md) | **12** | [13 →](PROMPT_13.md)

[← 12](PROMPT_12.md) | [index](README.md) | **13** | [14 →](PROMPT_14.md)

---

# PROMPT_13 — Phase 4 Step 4: devfs

**Session date:** 2026-03-29
**Status when starting:** Phase 4 Step 3 complete (tmpfs mounted on /, 64 inodes, kmalloc-backed files)
**Status when done:** Phase 4 Step 4 complete — /dev/null and /dev/zero live under tmpfs root

## What was built

- `src/devfs/devfs.h` — `devfs_init()`, `devfs_register(name, flags, ops)`, `mod_devfs`
- `src/devfs/devfs.c` — creates `/dev` dir in tmpfs root; static ops table (16 entries); `dev_finddir` wrapper patches the returned node's ops after tmpfs resolves the inode; built-in `/dev/null` (read→0, write→discard) and `/dev/zero` (read fills zeros)
- `src/tmpfs/tmpfs.h` — added `tmpfs_create()` and `tmpfs_inode_to_node()` as public API for devfs
- `src/tmpfs/tmpfs.c` — `tmpfs_create` made non-static; `tmpfs_inode_to_node` public wrapper added

## Key decisions

- **ops table + finddir wrapper** — tmpfs inodes don't carry a custom ops ptr, so devfs keeps a `{name → ops}` table and wraps `/dev` node's `finddir` to patch ops after resolution
- **`ptr` field stash** — the `/dev` vfs_node's `ptr` field stores the original tmpfs `fs_ops_t *` so the finddir wrapper can call through to the real implementation
- **`tmpfs_create` public** — devfs calls it to create inode entries under `/dev`; reusable by initrd/procfs later

## Verified serial output

```
[DEBUG] [devfs] registered /dev/null
[DEBUG] [devfs] registered /dev/zero
[INFO] [devfs] /dev ready (2 devices)
```

## Phase 4 complete ✓

1. ✅ ATA — PIO 28-bit LBA, primary+secondary channel probe, 100 MB disk
2. ✅ VFS — abstract layer, fs_ops vtable, fd table, vfs_mount
3. ✅ tmpfs — in-memory FS, 64 inodes, kmalloc-backed, mounted on /
4. ✅ devfs — /dev/null, /dev/zero, devfs_register extensible

## Next session

[PROMPT_14 →](PROMPT_14.md) — Process: TCB, kthread_create, idle pid=0, cpu_context_t.

---

[← 12](PROMPT_12.md) | [index](README.md) | **13** | [14 →](PROMPT_14.md)

# PROMPT_13 — Phase 4 Step 4: devfs

**Session date:** 2026-03-29
**Status when starting:** Phase 4 Step 3 complete (tmpfs mounted on /, 64 inodes, kmalloc-backed files)
**Status when done:** Phase 4 Step 4 complete — /dev/null and /dev/zero live under tmpfs root

## What was built

- `src/devfs/devfs.h` — `devfs_init()`, `devfs_register(name, flags, ops)`, `mod_devfs`
- `src/devfs/devfs.c` — creates `/dev` dir in tmpfs root; static ops table (16 entries); `dev_finddir` wrapper patches the returned node's ops after tmpfs resolves the inode; built-in `/dev/null` (read→0, write→discard) and `/dev/zero` (read fills zeros, write→discard)
- `src/tmpfs/tmpfs.h` — added `tmpfs_create()` and `tmpfs_inode_to_node()` as public API for devfs
- `src/tmpfs/tmpfs.c` — `tmpfs_create` made non-static; `tmpfs_inode_to_node` public wrapper added

## Key decisions

- **ops table + finddir wrapper** — tmpfs inodes don't carry a custom ops ptr, so devfs keeps a `{name → ops}` table and wraps the `/dev` node's `finddir` to patch the returned node's ops after resolution
- **`ptr` field stash** — the `/dev` vfs_node's `ptr` field is repurposed to store the original tmpfs `fs_ops_t *` so the finddir wrapper can call through to the real tmpfs implementation
- **tmpfs_create public** — devfs calls it to create inode entries under `/dev`; also usable by future initrd/procfs layers

## Verified serial output

```
[DEBUG] [devfs] registered /dev/null
[DEBUG] [devfs] registered /dev/zero
[INFO] [devfs] /dev ready (2 devices)
```

## Phase 4 complete

All storage/filesystem steps of Phase 4 are done:
1. ✅ ATA (PIO 28-bit LBA, primary+secondary channel probe, 100 MB disk)
2. ✅ VFS (abstract layer, fs_ops vtable, fd table, vfs_mount)
3. ✅ tmpfs (in-memory FS, 64 inodes, kmalloc-backed, mounted on /)
4. ✅ devfs (/dev/null, /dev/zero, devfs_register extensible)

## Next session prompt

Begin **Phase 5 Step 1**: `src/process/` — process/thread control blocks.

- `src/process/process_internal.h` — `PROC_MAX=64`, `KSTACK_SIZE=8192` (8 KB), process states (`PROC_UNUSED`, `PROC_READY`, `PROC_RUNNING`, `PROC_BLOCKED`, `PROC_ZOMBIE`), `cpu_context_t` (callee-saved regs: rbx, rbp, r12–r15, rsp, rip)
- `src/process/process.h` — `process_t` struct (pid, state, name[32], kstack, context, next), `process_init()`, `kthread_create(const char *name, void (*fn)(void))` → pid, `process_exit()`, `process_get_current()` → `process_t *`, `mod_process`
- `src/process/process.c`:
  - Static process table (64 slots); pid 0 = idle process (current boot context)
  - `kthread_create`: allocate slot, kmalloc 8 KB kernel stack, set up initial context (rip=fn, rsp=top of stack), state=READY
  - `process_exit()`: mark current ZOMBIE, yield to scheduler
  - `process_get_current()`: return pointer to current process slot
  - Log `[process] process subsystem ready, idle pid=0`
- Register `mod_process` after `mod_devfs` in module_registry
- Zero warnings policy applies

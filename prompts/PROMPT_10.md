# PROMPT_10 — Phase 4 Step 1: ATA

**Session date:** 2026-03-29
**Status when starting:** Phase 3 complete (Timer, Keyboard, PCI all working)
**Status when done:** Phase 4 Step 1 complete — ATA PIO driver, graceful no-drive detection

## What was built

- `src/ata/ata_internal.h` — primary channel ports (`ATA_PRIMARY_BASE=0x1F0`, `ATA_PRIMARY_CTRL=0x3F6`), register offsets, status bits (`BSY`, `DRQ`, `ERR`), commands (`READ=0x20`, `WRITE=0x30`, `FLUSH=0xE7`, `IDENTIFY=0xEC`), `ATA_DRIVE_MASTER=0xE0`, `ATA_SECTOR_SIZE=512`
- `src/ata/ata.h` — `ata_init()`, `ata_read(u32 lba, u8 count, void *buf)`, `ata_write(u32 lba, u8 count, const void *buf)`, `mod_ata`
- `src/ata/ata.c` — software reset via control register; IDENTIFY on init; `poll_bsy`/`poll_drq` with timeout; 28-bit LBA PIO read (loop `inw`) and write (loop `outw` + cache flush); sector count from IDENTIFY words 60–61

## Key decisions

- **Graceful no-drive** — IDENTIFY returns status 0x00 or sets ERR; log info and return 0 (not -1) so the module still marks initialized and boot continues cleanly
- **poll timeout** — 0x100000 iterations before giving up; avoids infinite hang on missing hardware
- **Cache flush after write** — ATA_CMD_FLUSH (0xE7) ensures data reaches disk before returning

## Verified serial output

```
[INFO] [ata] drive 0: 204800 sectors (100 MB)
```

## QEMU disk setup

CD-ROM must be at `index=0` (primary master, 0x1F0), disk at `index=1` (secondary master, 0x170).
Driver probes primary first — if ATAPI signature detected (mid=0x14, hi=0xEB) skips it and probes secondary.
`make disk` creates `build/disk.img` (100 MB zeroed). Makefile `run` target auto-creates it if missing.

## Next session prompt

Implement **Phase 4 Step 2**: `src/vfs/` — Abstract Virtual File System layer.

- `src/vfs/vfs_internal.h` — `VFS_MAX_FS=8`, `VFS_MAX_FDS=64`, `VFS_NAME_MAX=128`, `VFS_PATH_MAX=256`
- `src/vfs/vfs.h`:
  - `vfs_node_t` — abstract file/dir node: `name[VFS_NAME_MAX]`, `flags` (FILE/DIR/CHARDEV), `size`, `inode`, pointers to `fs_ops_t`
  - `fs_ops_t` — vtable: `read`, `write`, `open`, `close`, `readdir`, `finddir`
  - `filesystem_t` — `name`, `mount(const char *path)` → `vfs_node_t *`
  - `vfs_init()`, `filesystem_register(filesystem_t *)`, `vfs_mount(const char *fs_name, const char *path)`
  - `vfs_open`, `vfs_close`, `vfs_read`, `vfs_write`, `vfs_readdir`, `vfs_finddir`
  - File descriptor table: `vfs_fd_open(vfs_node_t *)` → int fd, `vfs_fd_read(int fd, ...)`, `vfs_fd_close(int fd)`
- Register `mod_vfs` after `mod_ata` in module_registry
- Zero warnings policy applies

[← 9](PROMPT_9.md) | [index](README.md) | **10** | [11 →](PROMPT_11.md)

---

# PROMPT_10 — Phase 4 Step 1: ATA

**Session date:** 2026-03-29
**Status when starting:** Phase 3 complete (Timer, Keyboard, PCI all working)
**Status when done:** Phase 4 Step 1 complete — ATA PIO driver, 100 MB disk detected

## What was built

- `src/ata/ata_internal.h` — primary channel ports (`ATA_PRIMARY_BASE=0x1F0`, `ATA_PRIMARY_CTRL=0x3F6`), secondary (`0x170`/`0x376`), register offsets, status bits (`BSY`, `DRQ`, `ERR`), commands (`READ=0x20`, `WRITE=0x30`, `FLUSH=0xE7`, `IDENTIFY=0xEC`), `ATA_DRIVE_MASTER=0xE0`, `ATA_SECTOR_SIZE=512`
- `src/ata/ata.h` — `ata_init()`, `ata_read(u32 lba, u8 count, void *buf)`, `ata_write(u32 lba, u8 count, const void *buf)`, `mod_ata`
- `src/ata/ata.c` — software reset via control register; IDENTIFY on init; `poll_bsy`/`poll_drq` with timeout; 28-bit LBA PIO read (loop `inw`) and write (loop `outw` + cache flush); sector count from IDENTIFY words 60–61

## Key decisions

- **IDENTIFY before checking ATAPI signature** — mid/hi are 0 for everything at reset time; send IDENTIFY first, then check mid=0x14/hi=0xEB to detect ATAPI
- **poll_drq skips BSY** — `if (s & ATA_SR_BSY) continue;` required or drive looks like it never has DRQ
- **Graceful no-drive** — IDENTIFY returns 0x00 status or sets ERR; log info and return 0 so boot continues cleanly
- **Secondary channel probe** — CD-ROM occupies primary (0x1F0); disk is on secondary (0x170)

## Verified serial output

```
[INFO] [ata] drive 0: 204800 sectors (100 MB)
```

## Next session

[PROMPT_11 →](PROMPT_11.md) — VFS: abstract layer, fs_ops vtable, fd table.

---

[← 9](PROMPT_9.md) | [index](README.md) | **10** | [11 →](PROMPT_11.md)

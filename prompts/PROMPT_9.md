# PROMPT_9 — Phase 3 Step 3: PCI

**Session date:** 2026-03-29
**Status when starting:** Phase 3 Step 2 complete (PS/2 keyboard, IRQ1 live)
**Status when done:** Phase 3 Step 3 complete — PCI bus enumerated, 6 QEMU devices found

## What was built

- `src/pci/pci_internal.h` — `PCI_CONFIG_ADDRESS=0xCF8`, `PCI_CONFIG_DATA=0xCFC`, `PCI_ADDR()` macro, all standard header offsets (`PCI_OFF_VENDOR`, `PCI_OFF_BAR0`…`BAR5`, etc.), `PCI_MAX_DEVICES=64`, `PCI_HTYPE_MULTI`
- `src/pci/pci.h` — `pci_device_t` struct (bus/slot/func, vendor_id, device_id, class/subclass/prog_if, int_line, bar[6]), `pci_init()`, `pci_find_device(u16 vendor, u16 device)`, `pci_read32()`, `pci_write32()`, `mod_pci`
- `src/pci/pci.c` — full bus 0–255 / slot 0–31 / func 0–7 enumeration; multi-function device support (header type bit 7); static device table up to 64 entries

## Key decisions

- **Multi-function check on func 0 only** — read header type from func 0; only probe funcs 1–7 if bit 7 is set, avoiding 7× spurious reads per non-MF slot
- **`%02x` → `%x`** — `kprintf` has no width-padding support; format strings use plain `%x`
- **config_read8/16 derived from config_read32** — PCI config space is only dword-accessible; byte/word reads shift the dword result

## Verified serial output

```
[INFO] [serial] COM1 initialized at 115200 baud
[INFO] [vga] 80x25 text mode ready
[INFO] [gdt] GDT loaded (5 entries)
[INFO] [idt] IDT loaded (256 entries)
[INFO] [pic] PIC remapped, IRQs 0-15 -> vectors 32-47
[INFO] [pmm] 510 MB free across 1 usable region(s)
[INFO] [vmm] VMM ready, CR3=0x101000
[INFO] [heap] slab allocator ready (10 caches, 8..4096 B)
[INFO] [timer] PIT channel 0 at 1000 Hz
[INFO] [keyboard] PS/2 keyboard ready
[DEBUG] [pci]   0:0.0 vendor=8086 device=1237 class=6:0
[DEBUG] [pci]   0:1.0 vendor=8086 device=7000 class=6:1
[DEBUG] [pci]   0:1.1 vendor=8086 device=7010 class=1:1
[DEBUG] [pci]   0:1.3 vendor=8086 device=7113 class=6:80
[DEBUG] [pci]   0:2.0 vendor=1234 device=1111 class=3:0
[DEBUG] [pci]   0:3.0 vendor=8086 device=100e class=2:0
[INFO] [pci] found 6 device(s)
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```

## Phase 3 complete

All 3 steps of Phase 3 are done:
1. ✅ Timer (PIT IRQ0, 1000 Hz, ksleep, callbacks)
2. ✅ Keyboard (PS/2 IRQ1, scancode→ASCII, circular buffer)
3. ✅ PCI (bus enumeration, pci_find_device, BAR reading)

## Next session prompt

Begin **Phase 4 Step 1**: `src/ata/` — ATA PIO storage driver.

- `src/ata/ata_internal.h` — ATA I/O port base (0x1F0 primary, 0x170 secondary), register offsets (DATA, ERROR, SECCOUNT, LBA_LO/MID/HI, DRIVE, STATUS, CMD), status bits (BSY, DRQ, ERR), commands (READ_SECTORS=0x20, WRITE_SECTORS=0x30, IDENTIFY=0xEC)
- `src/ata/ata.h` — `ata_init()`, `ata_read(u32 lba, u8 count, void *buf)` → int, `ata_write(u32 lba, u8 count, const void *buf)` → int, `mod_ata`
- `src/ata/ata.c`:
  - IDENTIFY drive on init; skip if no drive present
  - 28-bit LBA PIO read: poll BSY, send LBA+count, poll DRQ, read 256 words per sector via `insw` (or loop `inw`)
  - 28-bit LBA PIO write: poll BSY, send LBA+count, poll DRQ, write 256 words, flush with cache-flush command (0xE7)
  - Log `[ata] drive 0: N sectors (N MB)` on init, or `[ata] no drive detected`
- Register `mod_ata` after `mod_pci` in module_registry
- Zero warnings policy applies

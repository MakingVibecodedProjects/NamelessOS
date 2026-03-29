[← 8](PROMPT_8.md) | [index](README.md) | **9** | [10 →](PROMPT_10.md)

---

# PROMPT_9 — Phase 3 Step 3: PCI

**Session date:** 2026-03-29
**Status when starting:** Phase 3 Step 2 complete (PS/2 keyboard, IRQ1 live)
**Status when done:** Phase 3 Step 3 complete — PCI bus enumerated, 6 QEMU devices found

## What was built

- `src/pci/pci_internal.h` — `PCI_CONFIG_ADDRESS=0xCF8`, `PCI_CONFIG_DATA=0xCFC`, `PCI_ADDR()` macro, all standard header offsets, `PCI_MAX_DEVICES=64`, `PCI_HTYPE_MULTI`
- `src/pci/pci.h` — `pci_device_t` struct (bus/slot/func, vendor_id, device_id, class/subclass/prog_if, int_line, bar[6]), `pci_init()`, `pci_find_device(u16 vendor, u16 device)`, `pci_read32()`, `pci_write32()`, `mod_pci`
- `src/pci/pci.c` — full bus 0–255 / slot 0–31 / func 0–7 enumeration; multi-function device support (header type bit 7); static device table up to 64 entries

## Key decisions

- **Multi-function check on func 0 only** — read header type from func 0; only probe funcs 1–7 if bit 7 is set, avoiding 7× spurious reads per non-MF slot
- **`%x` not `%02x`** — `kprintf` has no width-padding; format strings use plain `%x`
- **config_read8/16 derived from config_read32** — PCI config space is dword-only; byte/word reads shift the result

## Verified serial output

```
[DEBUG] [pci]   0:0.0 vendor=8086 device=1237 class=6:0
[DEBUG] [pci]   0:1.0 vendor=8086 device=7000 class=6:1
[DEBUG] [pci]   0:1.1 vendor=8086 device=7010 class=1:1
[DEBUG] [pci]   0:1.3 vendor=8086 device=7113 class=6:80
[DEBUG] [pci]   0:2.0 vendor=1234 device=1111 class=3:0
[DEBUG] [pci]   0:3.0 vendor=8086 device=100e class=2:0
[INFO] [pci] found 6 device(s)
```

## Phase 3 complete ✓

1. ✅ Timer — PIT IRQ0, 1000 Hz, ksleep, callbacks
2. ✅ Keyboard — PS/2 IRQ1, scancode→ASCII, circular buffer
3. ✅ PCI — bus enumeration, pci_find_device, BAR reading

## Next session

[PROMPT_10 →](PROMPT_10.md) — ATA: PIO 28-bit LBA, ATAPI skip, 100 MB disk detected.

---

[← 8](PROMPT_8.md) | [index](README.md) | **9** | [10 →](PROMPT_10.md)

# PROMPT_8 — Phase 3 Step 2: Keyboard

**Session date:** 2026-03-29
**Status when starting:** Phase 3 Step 1 complete (PIT at 1000 Hz, IRQ0 live)
**Status when done:** Phase 3 Step 2 complete — PS/2 keyboard driver, IRQ1 live

## What was built

- `src/keyboard/keyboard_internal.h` — PS/2 port constants (`KB_DATA_PORT=0x60`, `KB_STATUS_PORT=0x64`), `KB_SC_MAX=0x58`, `KB_BUF_SIZE=256`, `KB_BUF_MASK`
- `src/keyboard/keyboard.h` — `keyboard_init()`, `keyboard_getc()` → i32, `mod_keyboard`
- `src/keyboard/keyboard.c` — 88-entry scancode set 1 → ASCII table; power-of-two circular buffer (256 B) with head/tail volatile indices; IRQ1 handler reads 0x60, ignores releases (bit 7) and 0xE0 extended prefix, translates and pushes to buffer; `keyboard_getc()` pops one byte or returns -1

## CLAUDE.md fixes this session

- Dependency graph corrected: gdt/idt/pic now shown above pmm/vmm (reflects actual init order)
- Full QEMU command updated from `-kernel` to `-cdrom` ISO boot (QEMU `-kernel` incompatible with Multiboot2)
- Cross-Compiler Flags section updated with all required flags: `-fno-pic -fno-pie -mno-sse -mno-sse2 -mno-avx -std=c11`

## Key decisions

- **Bit 7 = release** — scancode set 1 uses make/break codes; simply ignore anything with bit 7 set
- **0xE0 extended prefix** — extended keys (arrows, etc.) send 0xE0 then a second byte; ignore the prefix for now
- **Flush stale byte on init** — check status port bit 0; drain data port if set to prevent spurious first event

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
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```

## Next session prompt

Implement **Phase 3 Step 3**: `src/pci/` — PCI bus enumeration.

- `src/pci/pci_internal.h` — CONFIG_ADDRESS (0xCF8) / CONFIG_DATA (0xCFC) ports, config read/write helpers, PCI header field offsets (VENDOR, DEVICE, CLASS, SUBCLASS, BAR0–5, etc.)
- `src/pci/pci.h` — `pci_device_t` struct (bus, slot, func, vendor_id, device_id, class_code, subclass, prog_if, bar[6]), `pci_init()`, `pci_find_device(u16 vendor, u16 device)` → `pci_device_t *`, `pci_read_bar(pci_device_t *, u8 bar_idx)` → u32, `mod_pci`
- `src/pci/pci.c`:
  - Full bus enumeration: buses 0–255, slots 0–31, functions 0–7
  - Read vendor/device ID via CONFIG_ADDRESS; skip if 0xFFFF
  - Store up to 64 devices in a static table
  - `pci_find_device()`: linear scan of table
  - Log `[pci] found N device(s)` on init
- Register `mod_pci` after `mod_keyboard` in module_registry
- Zero warnings policy applies

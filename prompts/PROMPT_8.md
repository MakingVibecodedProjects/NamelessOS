[← 7](PROMPT_7.md) | [index](README.md) | **8** | [9 →](PROMPT_9.md)

---

# PROMPT_8 — Phase 3 Step 2: Keyboard

**Session date:** 2026-03-29
**Status when starting:** Phase 3 Step 1 complete (PIT at 1000 Hz, IRQ0 live)
**Status when done:** Phase 3 Step 2 complete — PS/2 keyboard driver, IRQ1 live

## What was built

- `src/keyboard/keyboard_internal.h` — PS/2 port constants (`KB_DATA_PORT=0x60`, `KB_STATUS_PORT=0x64`), `KB_SC_MAX=0x58`, `KB_BUF_SIZE=256`, `KB_BUF_MASK`
- `src/keyboard/keyboard.h` — `keyboard_init()`, `keyboard_getc()` → i32, `mod_keyboard`
- `src/keyboard/keyboard.c` — 88-entry scancode set 1 → ASCII table; power-of-two circular buffer (256 B) with head/tail volatile indices; IRQ1 handler reads 0x60, ignores releases (bit 7) and 0xE0 extended prefix, translates and pushes to buffer; `keyboard_getc()` pops one byte or returns -1

## Key decisions

- **Bit 7 = release** — scancode set 1 uses make/break codes; ignore anything with bit 7 set
- **0xE0 extended prefix** — extended keys send 0xE0 then a second byte; ignore the prefix for now
- **Flush stale byte on init** — check status port bit 0; drain data port if set to prevent spurious first event

## Verified serial output

```
[INFO] [keyboard] PS/2 keyboard ready
```

## Next session

[PROMPT_9 →](PROMPT_9.md) — PCI: bus enumeration, config space, 6 QEMU devices found.

---

[← 7](PROMPT_7.md) | [index](README.md) | **8** | [9 →](PROMPT_9.md)

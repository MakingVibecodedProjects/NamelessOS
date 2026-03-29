# PROMPT_7 — Phase 3 Step 1: Timer

**Session date:** 2026-03-29
**Status when starting:** Phase 2 complete (PMM, VMM, Heap all working)
**Status when done:** Phase 3 Step 1 complete — PIT firing at 1000 Hz, IRQ0 live

## What was built

- `src/timer/timer_internal.h` — PIT port constants (`PIT_CHANNEL0_DATA=0x40`, `PIT_COMMAND=0x43`), `PIT_BASE_HZ=1193182`, `TIMER_HZ=1000`, `PIT_DIVISOR=1193`, `TIMER_MAX_CALLBACKS=8`
- `src/timer/timer.h` — `timer_init()`, `timer_get_ticks()` → u64, `ksleep(u32 ms)`, `timer_register_callback(void (*fn)(void))`, `mod_timer`
- `src/timer/timer.c` — programs PIT channel 0 in rate-generator mode (mode 2) at 1000 Hz; `volatile u64 ticks` incremented in IRQ0 handler; callbacks array up to 8 entries; `ksleep` spins on tick counter with `pause`; registers via `irq_register(0, ...)` + `irq_enable(0)`

## Key decisions

- **Mode 2 (rate generator)** over mode 3 (square wave) — more precise for a tick source
- **`volatile u64 ticks`** — volatile required so GCC doesn't optimize away the `ksleep` spin loop
- **`pause` in ksleep** — reduces power consumption and avoids memory-order issues on the spin

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
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```

## Next session prompt

Implement **Phase 3 Step 2**: `src/keyboard/` — PS/2 keyboard driver.

- `src/keyboard/keyboard_internal.h` — PS/2 port constants (data=0x60, status=0x64), scancode set 1 table (128 entries → ASCII), `KB_BUF_SIZE=256`
- `src/keyboard/keyboard.h` — `keyboard_init()`, `keyboard_getc()` → i32 (-1 if empty), `mod_keyboard`
- `src/keyboard/keyboard.c`:
  - IRQ1 handler: read scancode from 0x60, ignore key-release (bit 7 set), translate make-code via table, push ASCII into 256-byte power-of-two circular buffer
  - `keyboard_getc()`: pop one byte from buffer, return -1 if empty
  - Register IRQ1 via `irq_register(1, ...)` + `irq_enable(1)`
  - Log `[keyboard] PS/2 keyboard ready`
- Register `mod_keyboard` in module_registry after `mod_timer`
- Zero warnings policy applies

[← 6](PROMPT_6.md) | [index](README.md) | **7** | [8 →](PROMPT_8.md)

---

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
[INFO] [timer] PIT channel 0 at 1000 Hz
```

## Next session

[PROMPT_8 →](PROMPT_8.md) — Keyboard: PS/2 IRQ1, scancode→ASCII, 256-byte ring buffer.

---

[← 6](PROMPT_6.md) | [index](README.md) | **7** | [8 →](PROMPT_8.md)

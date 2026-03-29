[← 30](PROMPT_30.md) | [index](README.md) | **31** | 32 →

---

# PROMPT_31 — Phase 8 Step 2: TTY

**Session date:** 2026-03-29
**Status when starting:** Phase 8 Step 1 complete (SMP infrastructure, BSP LAPIC, spinlocks)
**Status when done:** Phase 8 Step 2 complete — `/dev/tty0` registered in devfs, cooked mode line discipline, VGA echo; zero warnings

## What was built

- `src/tty/tty_internal.h` — `TTY_LBUF_SIZE=256` (cooked-mode line buffer), `TTY_RBUF_SIZE=1024` (read ring buffer)
- `src/tty/tty.h` — `tty_init` declaration, `mod_tty` extern
- `src/tty/tty.c`:
  - `lbuf[256]` + `lbuf_len`: accumulates raw keystrokes in cooked (canonical) mode
  - `rbuf[1024]` circular ring with `rbuf_head`/`rbuf_tail`; `rbuf_avail()`, `rbuf_push()`, `rbuf_pop()`
  - `tty_poll()`: registered as timer callback; drains `keyboard_getc()` every tick
    - `\r` or `\n` → flush lbuf to rbuf, append `\n`, echo `\n` to VGA
    - backspace / DEL (127) → erase last char from lbuf; send `\b`, ` `, `\b` to VGA
    - printable (0x20–0x7E) → echo to VGA and append to lbuf
    - control chars silently dropped
  - `tty_read()`: VFS read callback; drains rbuf, polls once if empty, stops at `\n`
  - `tty_write()`: VFS write callback; loops `vga_putchar` for each byte
  - `tty_ops`: `fs_ops_t { .read=tty_read, .write=tty_write }`
  - `tty_init_impl`: resets buffers, `timer_register_callback(tty_poll)`, `devfs_register("tty0", VFS_CHARDEV, &tty_ops)`
  - `tty_dump`: logs `lbuf_len` and `rbuf_avail`
- `src/core/module_registry.c` — added `#include "../tty/tty.h"` and `&mod_tty` after `&mod_devfs` (before `&mod_process`)

## Key decisions

- **Timer callback for polling** — TTY uses `timer_register_callback(tty_poll)` to drain the PS/2 keyboard ring on every 1 ms tick; no separate IRQ needed; follows same pattern as scheduler
- **No blocking read** — `tty_read` polls once when the buffer is empty, then returns what it has; blocking would require the scheduler to put the process to sleep, which needs a wait-queue mechanism not yet built
- **Module placement** — `mod_tty` must come after `mod_devfs` (needs `devfs_register`) but before `mod_process` (init will open `/dev/tty0` as fds 0/1/2)

## Verified build output

```
[INFO] [devfs] /dev/null and /dev/zero ready
[DEBUG] [devfs] registered /dev/tty0
[INFO] [tty] /dev/tty0 ready
[INFO] [process] process subsystem ready, idle pid=0
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

32 → — Phase 8 Step 3: launch init — embed init ELF, `elf_load` into new PML4, `iretq` to userspace PID 1

---

[← 30](PROMPT_30.md) | [index](README.md) | **31** | 32 →

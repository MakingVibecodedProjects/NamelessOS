[← 1](PROMPT_1.md) | [index](README.md) | **2** | [3 →](PROMPT_3.md)

---

# PROMPT_2 — Phase 1 Step 2: GDT

**Session date:** 2026-03-29
**Status when starting:** Phase 1 Step 1 complete (serial, VGA, module_registry booting)
**Status when done:** Phase 1 Step 2 complete — GDT loaded and TSS installed

## What was built

- `src/gdt/gdt_internal.h` — `gdt_entry_t` (u64), `tss64_t` (packed), `gdtr_t`, descriptor indices/selectors
- `src/gdt/gdt.h` — `gdt_init()`, `extern mod_gdt`
- `src/gdt/gdt.c` — descriptor bit constants, `gdt_set_tss()`, `gdt_load()` (lgdt + lretq CS reload + ltr), `mod_gdt`
- `src/core/module_registry.c` — added `mod_gdt` after `mod_vga`

## Key decisions

- **`-mno-sse -mno-sse2 -mno-avx` added to CFLAGS** — without these, GCC used `movaps`/`movdqa` to copy structs in `gdt_init`, causing a #GP fault before SSE is set up. Mandatory flag for all kernel code.
- `gdt_load()` uses three separate inline asm blocks: `lgdt`, far-`lretq` for CS reload, then `mov` for DS/ES/FS/GS/SS, then `ltr`
- TSS iomap_base set to `sizeof(tss64_t)` → no IOPM (all I/O ports forbidden from ring-3)

## Verified serial output

```
[INFO] [serial] COM1 initialized at 115200 baud
[INFO] [vga] 80x25 text mode ready
[INFO] [gdt] GDT loaded (5 entries)
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
```

## Next session

[PROMPT_3 →](PROMPT_3.md) — IDT: 256-entry interrupt descriptor table, exception stubs 0–31, register dump on fault.

---

[← 1](PROMPT_1.md) | [index](README.md) | **2** | [3 →](PROMPT_3.md)

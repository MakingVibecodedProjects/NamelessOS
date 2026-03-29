[← 4](PROMPT_4.md) | [index](README.md) | **5** | [6 →](PROMPT_6.md)

---

# PROMPT_5 — Phase 2 Step 1: PMM

**Session date:** 2026-03-29
**Status when starting:** Phase 1 complete (serial, VGA, GDT, IDT, PIC all working)
**Status when done:** Phase 2 Step 1 complete — PMM reporting 510 MB free

## What was built

- `boot/entry.asm` — save `ebx` (MB2 info ptr) to `mb2_info_phys` at start of `start:` before anything clobbers it; pass `edi=[mb2_info_phys]` as first arg to `kernel_main`; added `.bss.mb2` section for the 4-byte save slot
- `kernel.ld` — added `.bss.mb2` before `.bss.pagetables` (both in `.boot`); added `__kernel_start`/`__kernel_end` symbols around higher-half sections
- `src/pmm/pmm_internal.h` — `FRAME_SIZE`, bitmap macros, `pmm_state_t` (64-bit bitmap covering 4 GB = 1M frames = 16 KB bitmap), MB2 struct definitions
- `src/pmm/pmm.h` — `pmm_set_mb2()`, `pmm_init()`, `pmm_alloc_frame()`, `pmm_free_frame()`, `mod_pmm`
- `src/pmm/pmm.c` — init: all-used bitmap, walk MB2 mmap tag (type 6), mark usable regions free, re-mark frame 0 + kernel image + bitmap as used

## Key decisions

- **`pmm_set_mb2()` called before `modules_init_all()`** so `pmm_init()` already has the pointer when it runs
- **BSS alignment fix** — `mb2_info_phys` moved to its own `.bss.mb2` section; `.bss.pagetables` aligned via linker script `ALIGN(4K)`
- **Identity mapping** — MB2 struct accessed at physical address directly (first 4 GB identity mapped from entry.asm)

## Verified serial output

```
[INFO] [pmm] 510 MB free across 1 usable region(s)
```

## Next session

[PROMPT_6 →](PROMPT_6.md) — Heap: slab allocator, 10 size classes 8–4096 B, kmalloc/kfree/krealloc.

---

[← 4](PROMPT_4.md) | [index](README.md) | **5** | [6 →](PROMPT_6.md)

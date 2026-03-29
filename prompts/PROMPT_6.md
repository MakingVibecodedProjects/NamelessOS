# PROMPT_6 — Phase 2 Step 3: Heap

**Session date:** 2026-03-29
**Status when starting:** Phase 2 Step 2 complete (VMM, vmm_map_page/unmap/get_phys, page fault handler)
**Status when done:** Phase 2 Step 3 complete — slab allocator reporting 10 caches ready

## What was built

- `src/heap/heap_internal.h` — `slab_t` descriptor (lives at page start), `slab_cache_t` (partial/full lists), `free_obj_t` intrusive freelist node, `large_hdr_t` for >4096 byte allocs; constants `HEAP_MIN_ORDER=3`, `HEAP_MAX_ORDER=12`, `HEAP_NUM_CACHES=10`
- `src/heap/heap.h` — `heap_init()`, `kmalloc()`, `kzalloc()`, `krealloc()`, `kfree()`, `mod_heap`
- `src/heap/heap.c` — 10 slab caches (8, 16, 32 … 4096 bytes); each slab is one PMM frame with descriptor at page start and intrusive freelist over the rest; slabs returned to PMM when fully empty; large allocs fall through directly to `pmm_alloc_frame`

## Key decisions

- **Slab descriptor at page start** — `slab_of(ptr)` recovers it with `ptr & ~0xFFF`; no separate metadata pool needed
- **`kfree` slab detection** — checks `slab->obj_size` is within `[8, 4096]` range to distinguish slab from large alloc
- **Large alloc multi-page** — allocates pages individually from PMM; only first page freed on `kfree` (known limitation, acceptable until heap v2 with a proper large-alloc table)

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
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```

## Phase 2 complete

All 3 steps of Phase 2 are done:
1. ✅ PMM (MB2 mmap parser, 64-bit bitmap allocator, 510 MB free)
2. ✅ VMM (4-level page tables, map/unmap/get_phys, page fault handler)
3. ✅ Heap (slab allocator, 10 size classes, kmalloc/kfree/krealloc/kzalloc)

## Next session prompt

Begin **Phase 3 Step 1**: `src/timer/` — PIT timer driver.

- `src/timer/timer_internal.h` — PIT port constants, PIT_BASE_HZ=1193182, TIMER_HZ=1000, PIT_DIVISOR, TIMER_MAX_CALLBACKS
- `src/timer/timer.h` — `timer_init()`, `timer_get_ticks()` → u64, `ksleep(u32 ms)`, `timer_register_callback(void (*fn)(void))`, `mod_timer`
- `src/timer/timer.c`:
  - Program PIT channel 0 in rate-generator mode at 1000 Hz
  - IRQ0 handler: increment `volatile u64 ticks`, call all registered callbacks, send EOI
  - `ksleep(ms)`: spin until `ticks >= start + ms`
  - `timer_register_callback`: store fn pointer, up to TIMER_MAX_CALLBACKS
  - Register IRQ0 via `irq_register(0, ...)` and unmask with `irq_enable(0)`
  - Log `[timer] PIT channel 0 at 1000 Hz`
- Register `mod_timer` in module_registry after `mod_heap`
- Zero warnings policy applies

[← 5](PROMPT_5.md) | [index](README.md) | **6** | [7 →](PROMPT_7.md)

---

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
- **Large alloc multi-page** — allocates pages individually from PMM; only first page freed on `kfree` (known limitation, acceptable until heap v2)

## Verified serial output

```
[INFO] [heap] slab allocator ready (10 caches, 8..4096 B)
```

## Phase 2 complete ✓

1. ✅ PMM — MB2 mmap parser, 64-bit bitmap allocator, 510 MB free
2. ✅ VMM — 4-level page tables, map/unmap/get_phys, page fault handler
3. ✅ Heap — slab allocator, 10 size classes, kmalloc/kfree/krealloc/kzalloc

## Next session

[PROMPT_7 →](PROMPT_7.md) — Timer: PIT 1000 Hz, IRQ0, ksleep, callback system.

---

[← 5](PROMPT_5.md) | [index](README.md) | **6** | [7 →](PROMPT_7.md)

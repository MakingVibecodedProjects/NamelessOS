# PROMPT_5 — Phase 2 Step 1: PMM

**Session date:** 2026-03-29
**Status when starting:** Phase 1 complete (serial, VGA, GDT, IDT, PIC all working)
**Status when done:** Phase 2 Step 1 complete — PMM reporting 510 MB free

## What was built

- `boot/entry.asm` — save `ebx` (MB2 info ptr) to `mb2_info_phys` at very start of `start:` before anything clobbers it; pass `edi=[mb2_info_phys]` as first arg to `kernel_main`; added `.bss.mb2` section for the 4-byte save slot
- `kernel.ld` — added `.bss.mb2` before `.bss.pagetables` (both in `.boot`); added `__kernel_start`/`__kernel_end` symbols around higher-half sections
- `src/pmm/pmm_internal.h` — `FRAME_SIZE`, bitmap macros (`bitmap_set/clear/test`), `pmm_state_t` (64-bit bitmap covering 4 GB = 1M frames = 16 KB bitmap), MB2 struct definitions (`mb2_header_t`, `mb2_tag_t`, `mb2_tag_mmap_t`, `mb2_mmap_entry_t`)
- `src/pmm/pmm.h` — `pmm_set_mb2()`, `pmm_init()`, `pmm_alloc_frame()`, `pmm_free_frame()`, `mod_pmm`
- `src/pmm/pmm.c` — init: all-used bitmap, walk MB2 mmap tag (type 6), mark usable regions free, re-mark frame 0 + kernel image + bitmap as used
- `src/core/kernel.c` — new signature `kernel_main(u64 mb2_info_phys)`, calls `pmm_set_mb2()` before `modules_init_all()`

## Key decisions

- **`pmm_set_mb2()` called before `modules_init_all()`** so `pmm_init()` already has the pointer when it runs as a module
- **BSS alignment warning fix** — `ALIGN 4096` in `nobits` section caused NASM warning; moved `mb2_info_phys` to its own tiny `.bss.mb2` section, page-aligned `.bss.pagetables` via linker script `ALIGN(4K)` in `.boot`
- **Identity mapping** — MB2 struct accessed at physical address directly (first 4 GB identity mapped since entry.asm)
- Bitmap covers up to 4 GB (PMM_MAX_FRAMES = 1M); larger RAM support can be added in Phase 2 Step 3

## Verified serial output

```
[INFO] [serial] COM1 initialized at 115200 baud
[INFO] [vga] 80x25 text mode ready
[INFO] [gdt] GDT loaded (5 entries)
[INFO] [idt] IDT loaded (256 entries)
[INFO] [pic] PIC remapped, IRQs 0-15 -> vectors 32-47
[INFO] [pmm] 510 MB free across 1 usable region(s)
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```

## Next session prompt

Implement **Phase 2 Step 2**: `src/vmm/` — Virtual Memory Manager.

- `src/vmm/vmm_internal.h` — page table entry flags (PRESENT, WRITE, USER, HUGE, NX), `pml4e_t/pdpte_t/pde_t/pte_t` as u64 typedefs, `PAGE_SIZE`, `virt_to_pml4_idx` etc. macros
- `src/vmm/vmm.h` — `vmm_init()`, `vmm_map_page(u64 virt, u64 phys, u64 flags)`, `vmm_unmap_page(u64 virt)`, `vmm_get_phys(u64 virt)` → u64, `extern mod_vmm`
- `src/vmm/vmm.c`:
  - `vmm_init()`: take ownership of the existing PML4 set up in `entry.asm` (CR3), store its VA, log `[vmm] VMM ready, CR3=0x...`
  - `vmm_map_page()`: walk/allocate PML4→PDPT→PDT→PT using `pmm_alloc_frame()` for missing tables; install the PTE; `invlpg`
  - `vmm_unmap_page()`: clear PTE, `invlpg`
  - `vmm_get_phys()`: walk tables, return physical address or 0 if not mapped
  - Page fault handler (#PF, vector 14): install via `idt_set_gate`; log fault address (CR2) + error code; call `PANIC` for now
- Register `mod_vmm` in module_registry after `mod_pmm`
- Log `[vmm] VMM ready, CR3=0x%x` on init
- Zero warnings policy applies

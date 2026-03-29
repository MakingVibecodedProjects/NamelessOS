[← 16](PROMPT_16.md) | [index](README.md) | **17** | [18 →](PROMPT_18.md)

---

# PROMPT_17 — Phase 6 Step 1: Per-process page tables + COW fork

**Session date:** 2026-03-29
**Status when starting:** Phase 5 complete (process, scheduler, syscall all live)
**Status when done:** Phase 6 Step 1 complete — per-process PML4s, COW fork, CR3 switch on context switch

## What was built

- `src/vmm/vmm_internal.h` — added `PTE_COW` (bit 9, OS-defined copy-on-write flag)
- `src/vmm/vmm.h` — added `vmm_create_user_pml4()`, `vmm_map_user_page()`, `vmm_fork_pml4()`, `vmm_destroy_user_pml4()`, `vmm_switch_to()`
- `src/vmm/vmm.c` — implemented all five new functions:
  - `vmm_create_user_pml4`: allocates a PML4, zeros user half, copies kernel half (entries 256–511) verbatim from boot PML4
  - `vmm_map_user_page`: walks/allocates page tables in a target PML4; only fires `invlpg` if the target PML4 is currently loaded in CR3
  - `vmm_fork_pml4`: COW-clones user half — deep-copies PDPT→PDT→PT frames; marks all writable PTEs as read-only + `PTE_COW` in both parent and child; flushes parent's TLB by reloading CR3
  - `vmm_destroy_user_pml4`: frees all user-half PT/PDT/PDPT frames then the PML4 frame; does not free the mapped data pages (caller's responsibility)
  - `vmm_switch_to`: writes CR3 only if changing address space (avoids unnecessary TLB flush)
  - `cow_handle`: called from page fault handler on `err=3` (present+write); walks current page tables, finds the COW PTE, allocates a new frame, `memcpy`s the old content, remaps writable
- `src/process/process.h` — added `pml4_phys u64` field to `process_t`; added `process_fork()` declaration
- `src/process/process.c` — `pml4_phys = 0` for idle and new threads (0 = kernel address space); `process_fork()`: allocates child TCB + kstack, promotes parent PML4 if needed, calls `vmm_fork_pml4`, copies parent `cpu_context_t`, calls `scheduler_add`
- `src/scheduler/scheduler.h` — exposed `scheduler_add(process_t *)` as public API
- `src/scheduler/scheduler.c` — `scheduler_yield` calls `vmm_switch_to(next->pml4_phys)` before `context_switch` when `next->pml4_phys != 0`
- `src/syscall/syscall.c` — `sys_fork` wired: calls `process_fork()`, returns child pid to parent

## Key decisions

- **`pml4_phys = 0` means kernel address space** — avoids a mandatory PML4 alloc for every kernel thread; scheduler skips `vmm_switch_to` when the field is zero
- **Kernel half shared by pointer copy, not reference counting** — all processes share the same kernel PML4 entries; kernel mappings added after boot are visible to all processes automatically
- **COW at PT level only** — huge pages (PDPT/PDT entries with PTE_HUGE) are shared read-only but not COW-tracked; fine for now since user memory will always be 4 KB mapped
- **`vmm_destroy_user_pml4` frees table frames, not data pages** — COW pages may be shared; the caller (process teardown in Phase 6 Step 3) must walk PTEs and `pmm_free_frame` data pages whose ref count reaches zero. Ref counting is Phase 6 Step 3.
- **`process_fork` promotes parent PML4** — if parent still has `pml4_phys = 0` (kernel space only), it gets a proper user PML4 before forking, so both parent and child have independent address spaces going forward

## Verified serial output

```
[INFO] [vmm] VMM ready, CR3=0x101000
[INFO] [process] process subsystem ready, idle pid=0
[INFO] [scheduler] round-robin scheduler ready
[INFO] [syscall] SYSCALL/SYSRET ready, 320 slots
[INFO] [kernel] All modules initialized.
```

## Next session

[PROMPT_18 →](PROMPT_18.md) — Phase 6 Step 2: ELF64 loader (`src/elf/`).


---

[← 16](PROMPT_16.md) | [index](README.md) | **17** | [18 →](PROMPT_18.md)

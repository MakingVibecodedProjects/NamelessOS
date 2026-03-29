[← 17](PROMPT_17.md) | [index](README.md) | **18** | 19 →

---

# PROMPT_18 — Phase 6 Step 2: ELF64 Loader

**Session date:** 2026-03-29
**Status when starting:** Phase 6 Step 1 complete (per-process PML4s, COW fork, CR3 switch)
**Status when done:** Phase 6 Step 2 complete — ELF64 loader parses and maps PT_LOAD segments into any process address space

## What was built

- `src/elf/elf_internal.h` — `elf64_hdr_t`, `elf64_phdr_t` packed structs; ELF magic/class/data/type/machine constants; `PT_LOAD`, `PF_X/W/R` flags; `ELF_MAX_PHDRS=16` sanity limit
- `src/elf/elf.h` — `elf_load(buf, buf_size, pml4_phys, entry_out)`, `mod_elf`
- `src/elf/elf.c` — `elf_check` validates magic/class/endian/type/machine/phdr bounds; `elf_load_segment` allocates one frame per page of `p_memsz`, zero-fills (BSS), copies file data slice, calls `vmm_map_user_page`; `elf_load` iterates program headers, skips non-`PT_LOAD`, calls `elf_load_segment` for each, returns `e_entry`; `mod_elf` has no `init` (pure library)

## Key decisions

- **No `init` function** — `mod_elf` is registered for `dump` visibility only; `elf_load` is a stateless library call with no global state to set up
- **Frame-per-page loop with slice copy** — for each 4 KB page in the segment's virtual range, compute the intersection with `[p_vaddr, p_vaddr+p_filesz)` to copy file bytes, zero the rest; handles both aligned and unaligned `p_vaddr` correctly
- **PTE flags from `p_flags`** — `PF_W` → `PTE_WRITE`; no `PF_X` → `PTE_NX`; always `PTE_USER`; no `PTE_WRITE` on code segments
- **`buf_size` bounds on every access** — `p_offset + p_filesz` checked against `buf_size` before any pointer arithmetic; prevents out-of-bounds reads on truncated images
- **`vmm_map_user_page` is target-PML4-aware** — the loader works on any process's address space without switching CR3; it only fires `invlpg` when the target PML4 is currently active

## Verified serial output

```
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```
(ELF loader produces output only when `elf_load` is called; no self-test at boot)

## Next session

[PROMPT_19 →](PROMPT_19.md) — Phase 6 Step 3: userspace libc (`userspace/libc/`): crt0, syscall wrappers, malloc, printf, string.h.

---

[← 17](PROMPT_17.md) | [index](README.md) | **18** | 19 →

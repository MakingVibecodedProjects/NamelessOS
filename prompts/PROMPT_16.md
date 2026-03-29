[← 15](PROMPT_15.md) | [index](README.md) | **16** | [17 →](PROMPT_17.md)

---

# PROMPT_16 — Phase 5 Step 3: Syscall

**Session date:** 2026-03-29
**Status when starting:** Phase 5 Step 2 complete (scheduler, round-robin, context_switch live)
**Status when done:** Phase 5 Step 3 complete — SYSCALL/SYSRET dispatch table, user GDT segments, 12 syscalls wired

## What was built

- `src/gdt/gdt_internal.h` — added `GDT_USER_DATA` (index 3, 0x18), `GDT_USER_CODE` (index 4, 0x20), shifted TSS to indices 5–6 (0x28); added `SEG_USER_DATA`/`SEG_USER_CODE` selectors; added `STAR_KERNEL_CS`/`STAR_USER_BASE` constants for MSR programming
- `src/gdt/gdt.c` — added `DESC_USER_DATA`/`DESC_USER_CODE` descriptors (DPL=3); GDT now 7 entries; log line updated to show user seg selectors
- `src/syscall/syscall_internal.h` — MSR addresses (EFER/STAR/LSTAR/SFMASK), SFMASK_VALUE (IF|DF), Linux-compatible syscall numbers, error codes (ENOSYS/EBADF/EINVAL), `SYSCALL_MAX=320`
- `src/syscall/syscall.h` — `syscall_init()`, `mod_syscall`
- `src/syscall/syscall_entry.asm` — SYSCALL landing pad: pushes rcx/r11, shuffles args (rdi←rax, rsi←rdi, rdx←rsi, rcx←rdx, r8←r10, r9←r8), STI, calls `syscall_dispatch`, CLI, pops r11/rcx, `o64 sysret`
- `src/syscall/syscall.c` — `wrmsr`/`rdmsr` helpers; 320-slot dispatch table init to `sys_enosys`; handlers for read/write/open/close/getpid/exit; stubs (ENOSYS) for fork/execve/waitpid/mmap/munmap/brk; `syscall_init` programs EFER/STAR/LSTAR/SFMASK

## Key decisions

- **User segments inserted before TSS** — SYSRET64 derives SS/CS from `STAR[63:48]`; the layout `0x18=user_data, 0x20=user_code` gives STAR_USER_BASE=0x10 so SYSRET SS=0x18|3=0x1B ✓ and CS=0x20|3=0x23 ✓
- **Arg shuffle in ASM before the C call** — rcx is overwritten by the `call` instruction; the user-RIP saved in rcx must be moved to `push rcx` first, then rcx used for a3
- **`o64 sysret` prefix** — forces the 64-bit SYSRET form; without `o64` NASM emits the 32-bit variant which returns to 32-bit compatibility mode
- **`open` is a stub returning -EINVAL** — path resolution requires per-process CWD which belongs to Phase 6; wired now so the syscall number is reserved
- **`sys_enosys` as table default** — all 320 slots initialised to `sys_enosys` before overwriting known entries; unknown syscalls log a warning and return -ENOSYS

## Verified serial output

```
[INFO] [gdt] GDT loaded (7 entries, user segs at 0x1b/0x23)
[INFO] [syscall] SYSCALL/SYSRET ready, 320 slots
[INFO] [kernel] All modules initialized.
```

## Next session

[PROMPT_17 →](PROMPT_17.md) — Phase 6: per-process page tables, COW fork, ELF64 loader.

---

[← 15](PROMPT_15.md) | [index](README.md) | **16** | [17 →](PROMPT_17.md)

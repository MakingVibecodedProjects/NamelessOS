← [index](README.md) | **1** | [2 →](PROMPT_2.md)

---

# PROMPT_1 — Phase 1 Step 1: Full Kernel Scaffold

**Session date:** 2026-03-29
**Status when starting:** nothing built
**Status when done:** Phase 1 Step 1 complete, booting in QEMU

## What was built

- Git repo initialized with `.gitignore`, `README.md`, `LICENSE`
- `Makefile` with `make`, `make iso`, `make run`, `make debug`, `make clean`
- `kernel.ld` — higher-half kernel at 0xFFFFFFFF80000000, boot section at 0x100000
- `boot/entry.asm` — Multiboot2 header, 32→64-bit long mode, 1 GB page identity+higher-half map
- `grub.cfg` + `make iso` target (GRUB2 multiboot2 ISO via `grub-mkrescue`)
- `src/lib/types.h` — u8/u16/u32/u64/usize/i8–i64/bool/NULL
- `src/lib/module.h` — `kernel_module_t` struct (shared, no circular deps)
- `src/lib/string.h/.c` — memset/memcpy/memmove/memcmp/strlen/strcmp/strcpy/strncpy/strncmp
- `src/lib/printf.h/.c` — `kprintf` + `vkprintf` with `%s %d %u %x %X %p %c %%`
- `src/serial/serial.h/.c` — COM1 115200 baud, `klog(level, fmt, ...)`, `mod_serial`
- `src/vga/vga.h/.c` — 80×25 text mode, scroll, cursor, `mod_vga`
- `src/core/panic.h` — `PANIC(fmt, ...)` macro
- `src/core/module_registry.h/.c` — `modules_init_all()`, `modules_dump_all()`
- `src/core/kernel.c` — `kernel_main()` entry point

## Key decisions

- **`-fno-pic -fno-pie`** required for `x86_64-linux-gnu-gcc` with `-mcmodel=kernel`
- **QEMU `-kernel` does not work** for Multiboot2 ELF; boot via GRUB2 ISO instead
- **Page tables** in `.bss.pagetables nobits` section within the low `.boot` section so 32-bit code can load their physical addresses in 32-bit registers
- **1 GB pages** (PDPE_PS) used for the PDPT level — simpler than 2 MB pages, no PDT needed
- **`kernel_module_t`** moved to `src/lib/module.h` to break circular include between `serial.h`/`vga.h` and `module_registry.h`

## Verified serial output

```
[INFO] [serial] COM1 initialized at 115200 baud
[INFO] [vga] 80x25 text mode ready
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
```

## Next session

[PROMPT_2 →](PROMPT_2.md) — GDT: flat 64-bit descriptors, TSS stub, segment reload.

---

← [index](README.md) | **1** | [2 →](PROMPT_2.md)

[← 2](PROMPT_2.md) | [index](README.md) | **3** | [4 →](PROMPT_4.md)

---

# PROMPT_3 — Phase 1 Step 3: IDT

**Session date:** 2026-03-29
**Status when starting:** Phase 1 Step 2 complete (GDT loaded)
**Status when done:** Phase 1 Step 3 complete — IDT loaded, exceptions 0–31 handled, smoke-tested

## What was built

- `src/idt/idt_internal.h` — `idt_entry_t` (16-byte packed gate), `idtr_t`, `IDT_GATE_INTR`, `IDT_KERNEL_CS`
- `src/idt/idt.h` — `idt_init()`, `idt_set_gate(u8, void*, u8)`, `extern mod_idt`
- `src/idt/idt.c` — fills 256-entry IDT, loads with `lidt`, `mod_idt`
- `src/idt/exceptions.asm` — NASM stubs for vectors 0–31 (ISR_NOERR/ISR_ERR macros), `isr_table` in `.rodata`, common `exception_common` trampoline
- `src/idt/exceptions.c` — `exception_dispatch()`: prints vec/name/error/RIP/CS/RFLAGS/RSP to serial then VGA, halts
- Updated `Makefile` to assemble `src/**/*.asm` → `build/asm/**/*.o` (separate dir to avoid `.c`/`.asm` name collision)

## Key decisions

- **`build/asm/` prefix for src ASM objects** — `src/idt/exceptions.asm` would collide with `src/idt/exceptions.c` at `build/idt/exceptions.o`. Fixed by routing ASM output to `build/asm/idt/exceptions.o`.
- Exception trampoline pops `vector` and `error_code` into `rdi`/`rsi`, reads the CPU-pushed frame from RSP, then `sub rsp, 8` for 16-byte alignment before `call exception_dispatch`.

## Verified serial output (normal boot)

```
[INFO] [serial] COM1 initialized at 115200 baud
[INFO] [vga] 80x25 text mode ready
[INFO] [gdt] GDT loaded (5 entries)
[INFO] [idt] IDT loaded (256 entries)
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
```

## Exception handler verified (ud2 smoke test)

```
[INFO] [kernel] Triggering #UD to test IDT...

*** CPU EXCEPTION ***
  Vec    : 6 (Invalid Opcode)
  Error  : 0x0
  RIP    : 0x80105148
  CS     : 0x8
  RFLAGS : 0x86
  RSP    : 0x8010cff0
```

## Next session

[PROMPT_4 →](PROMPT_4.md) — PIC: 8259A remap, IRQ stubs 0–15, irq_register/enable/disable, STI.

---

[← 2](PROMPT_2.md) | [index](README.md) | **3** | [4 →](PROMPT_4.md)

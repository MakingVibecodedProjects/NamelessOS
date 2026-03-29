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
- Updated `Makefile` to also assemble `src/**/*.asm` → `build/asm/**/*.o` (separate dir to avoid `.c`/`.asm` name collision)

## Key decisions / bugs fixed

- **`build/asm/` prefix for src ASM objects** — `src/idt/exceptions.asm` would produce `build/idt/exceptions.o`, colliding with `src/idt/exceptions.c` → `build/idt/exceptions.o`. Fixed by outputting ASM to `build/asm/idt/exceptions.o`.
- Exception trampoline pops `vector` and `error_code` into `rdi`/`rsi`, then reads the CPU-pushed frame directly from RSP to fill `rdx`/`rcx`/`r8`/`r9`, then `sub rsp, 8` for 16-byte alignment before `call exception_dispatch`.

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

## Next session prompt

Implement **Phase 1 Step 4**: `src/pic/` — 8259A PIC remapping and IRQ management.

- `src/pic/pic_internal.h` — PIC1/PIC2 port constants, ICW1–4 values, EOI command
- `src/pic/pic.h` — `pic_init()`, `irq_register(u8 irq, void (*handler)(void))`, `irq_enable(u8)`, `irq_disable(u8)`, `pic_eoi(u8)`, `extern mod_pic`
- `src/pic/pic.c` — remap IRQ 0–7 to vectors 32–39, IRQ 8–15 to 40–47; mask all IRQs after init; handler table; `irq_dispatch()` called from IRQ stubs
- `src/pic/irq_stubs.asm` — stubs for vectors 32–47 (no error code, push IRQ number, call `irq_dispatch`), save/restore all registers
- Register `mod_pic` in module_registry after `mod_idt`
- After `pic_init()`, enable `STI` in `kernel_main` and log `[kernel] Interrupts enabled`
- Log `[pic] PIC remapped, IRQs 0-15 → vectors 32-47` on init
- Zero warnings policy applies

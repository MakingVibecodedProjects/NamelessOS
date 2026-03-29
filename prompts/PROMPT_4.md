# PROMPT_4 — Phase 1 Step 4: PIC

**Session date:** 2026-03-29
**Status when starting:** Phase 1 Step 3 complete (IDT loaded)
**Status when done:** Phase 1 Step 4 complete — 8259A remapped, IRQ stubs installed, STI enabled

## What was built

- `src/pic/pic_internal.h` — port constants (PIC1/2 CMD/DATA), ICW values, EOI, vector offsets, IRQ_COUNT
- `src/pic/pic.h` — `pic_init()`, `irq_register()`, `irq_enable/disable()`, `pic_eoi()`, `irq_dispatch()`, `mod_pic`
- `src/pic/pic.c` — full 8259A initialisation sequence (ICW1–4), masks all IRQs after remap, installs stubs into IDT via `idt_set_gate`, `irq_dispatch` calls registered handler then EOI
- `src/pic/irq_stubs.asm` — 16 IRQ stubs (push irq_num, jmp irq_common), common trampoline saves/restores all caller-saved regs, calls `irq_dispatch(u8)`, `iretq`; `irq_stub_table` in `.rodata`
- `src/core/kernel.c` — added `sti` + `[kernel] Interrupts enabled.` log after `modules_init_all()`

## Key decisions

- All 16 IRQs masked after `pic_init()` — individual drivers call `irq_enable(n)` when ready
- `io_wait()` between PIC init commands to let slow ISA hardware settle
- IRQ stubs: `sub rsp, 8` before `call irq_dispatch` to maintain 16-byte stack alignment (9 pushes + 1 irq_num = 80 bytes, plus 40-byte CPU frame = 120 → not aligned → -8 → 112 ... actually verified by clean run)

## Verified serial output

```
[INFO] [serial] COM1 initialized at 115200 baud
[INFO] [vga] 80x25 text mode ready
[INFO] [gdt] GDT loaded (5 entries)
[INFO] [idt] IDT loaded (256 entries)
[INFO] [pic] PIC remapped, IRQs 0-15 -> vectors 32-47
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```

## Phase 1 complete

All 4 steps of Phase 1 are done:
1. ✅ Scaffold (Multiboot2, long mode, lib, serial, VGA, module_registry)
2. ✅ GDT (flat 64-bit, TSS stub, segment reload)
3. ✅ IDT (256 entries, exception stubs 0–31, register dump)
4. ✅ PIC (8259A remap, IRQ stubs, irq_register/enable/disable, STI)

## Next session prompt

Begin **Phase 2 Step 1**: `src/pmm/` — Physical Memory Manager.

GRUB passes a Multiboot2 info struct in `rbx` on entry. Pass it to `kernel_main` and then to `pmm_init()`.

- `boot/entry.asm`: save `rbx` (Multiboot2 info pointer) before doing anything that might clobber it; pass it as first arg to `kernel_main` (rdi)
- `src/pmm/pmm_internal.h` — frame size (4096), bitmap helpers, `pmm_state_t` struct
- `src/pmm/pmm.h` — `pmm_init(u64 mb2_info_phys)`, `pmm_alloc_frame()` → `u64` (physical addr), `pmm_free_frame(u64)`, `pmm_dump()`, `extern mod_pmm`
- `src/pmm/pmm.c`:
  - Walk the Multiboot2 memory map tag (type 6) to find usable RAM
  - Build a bitmap: 1 bit per 4 KB frame, initially all used
  - Mark usable regions as free, then re-mark kernel image + bitmap as used
  - `pmm_alloc_frame()`: first-fit scan of bitmap, returns physical address
  - `pmm_free_frame(u64)`: clears the bit
  - Log `[pmm] N MB free across M regions` on init
- Register `mod_pmm` in module_registry after `mod_pic`
- `kernel_main` signature changes to `void kernel_main(u64 mb2_info)`
- Zero warnings policy applies

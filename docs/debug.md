# Debugging with GDB + QEMU

## Start in debug mode

```bash
make debug
```

QEMU launches with `-s -S`: it opens GDB stub on port 1234 and freezes before
the first instruction. The terminal hangs waiting for GDB to connect.

## Connect GDB

In a second terminal (WSL or Linux):

```bash
gdb build/namelessos.elf
```

Inside GDB:

```gdb
(gdb) target remote localhost:1234
(gdb) layout asm
(gdb) break kernel_main
(gdb) continue
```

## Useful GDB commands

```gdb
info registers          # dump all registers
x/20i $rip              # disassemble 20 instructions at RIP
x/10gx $rsp             # dump 10 qwords at RSP (stack)
p proc_table[0]         # inspect process table entry
set scheduler-locking on  # freeze all CPUs while stepping (SMP later)
```

## Symbols

The ELF is built with `-O2` but **without** `-g` by default (kernel must be small).
To add debug info for a session:

```bash
make CFLAGS_EXTRA="-g -O0" iso
```

## QEMU monitor

To reach the QEMU monitor while running (useful for inspecting memory, devices):

```
Ctrl-A C    # switch terminal between serial and QEMU monitor
```

In the monitor:

```
info registers
xp /10x 0xffff800000101000   # physical memory peek
```

## Common issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Triple fault on boot | Bad GDT/IDT or stack | Attach GDB, break on `_start` |
| Page fault at 0x0 | NULL pointer deref | `x/i $rip` to find the offending instruction |
| `movaps` fault | SSE not disabled | Check `-mno-sse -mno-sse2 -mno-avx` in CFLAGS |
| Serial output stops | IRQs masked | Verify `sti` in PIC init, check `irq_enable(0)` |

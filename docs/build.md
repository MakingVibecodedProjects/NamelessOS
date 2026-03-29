# Build System

## Targets

| Target | What it does |
|--------|-------------|
| `make` | Compile all `.c` and `.asm` sources → link `build/namelessos.elf` |
| `make iso` | Wrap ELF in a GRUB2 Multiboot2 ISO → `build/namelessos.iso` |
| `make run` | Build ISO if needed, create `disk.img` if needed, boot in QEMU |
| `make debug` | Same as `run` but adds `-s -S` — QEMU pauses at startup, GDB on `:1234` |
| `make disk` | Create a blank 100 MB raw disk image at `build/disk.img` |
| `make clean` | Delete the entire `build/` directory |

## How the build works

```
src/**/*.c   ──── x86_64-linux-gnu-gcc ────┐
src/**/*.asm ──── nasm -f elf64 ───────────┼──► x86_64-linux-gnu-ld ──► namelessos.elf
boot/*.asm   ──── nasm -f elf64 ───────────┘          (kernel.ld)
```

Then:

```
namelessos.elf + grub.cfg ──── grub-mkrescue ──► namelessos.iso
```

## Compiler flags (kernel)

```
-ffreestanding   no stdlib assumptions
-nostdlib        don't link against any library
-nostdinc        don't search system include paths
-mno-red-zone    no 128-byte red zone below RSP (IRQ handlers need this)
-mcmodel=kernel  kernel memory model (addresses above 0xFFFFFFFF80000000)
-fno-pic         no position-independent code
-fno-pie         no position-independent executable
-mno-sse         \
-mno-sse2         } no SIMD — GCC emits movaps without these, causing
-mno-avx         /  faults before SSE state is saved
-O2              optimise
-Wall -Wextra    treat all warnings as errors in practice (zero-warnings policy)
-std=c11         C11
```

## QEMU command (full)

```bash
qemu-system-x86_64 \
  -drive file=build/disk.img,format=raw,if=ide,index=0 \
  -drive file=build/namelessos.iso,format=raw,media=cdrom,index=1 \
  -m 512M \
  -serial stdio \
  -netdev user,id=net0 -device e1000,netdev=net0 \
  -display none \
  -boot d
```

- `index=0` (primary master) = disk image — ATA driver probes here
- `index=1` (secondary) = CD-ROM — GRUB boots from here
- `-display none` = headless; all output over serial (stdout)
- `-boot d` = boot from CD

## Why not `-kernel`?

GRUB2 uses the Multiboot2 protocol and expects to load an ELF from a CD image.
Passing the ELF directly with `-kernel` bypasses GRUB and does not set up the
Multiboot2 info structure the kernel expects. Always use `-cdrom` / ISO boot.

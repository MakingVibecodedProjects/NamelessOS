# NamelessOS

A monolithic x86_64 kernel written in C from scratch, vibecoded with Claude Code.

![Status](https://img.shields.io/badge/status-work%20in%20progress-yellow)
![License: MIT](https://img.shields.io/badge/license-MIT-blue)

## What's built so far

| Phase | Module | Status |
|-------|--------|--------|
| 1 | Multiboot2 boot → 64-bit long mode | done |
| 1 | GDT, IDT, 8259A PIC | done |
| 2 | Physical memory manager (bitmap allocator) | done |
| 2 | Virtual memory manager (4-level page tables) | done |
| 2 | Slab allocator (kmalloc / kfree / krealloc) | done |
| 3 | PIT timer @ 1000 Hz + callback system | done |
| 3 | PS/2 keyboard (scancode→ASCII, ring buffer) | done |
| 3 | PCI bus enumeration | done |
| 4 | ATA PIO 28-bit LBA driver | done |
| 4 | VFS abstract layer (vtable dispatch, fd table) | done |
| 4 | tmpfs (in-memory filesystem, mounted on /) | done |
| 4 | devfs (/dev/null, /dev/zero) | done |
| 5 | Process subsystem (TCB, kthread_create, idle pid=0) | done |
| 5 | Preemptive round-robin scheduler | done |
| 5 | SYSCALL/SYSRET dispatch table | done |
| 6 | Per-process page tables, COW fork | done |
| 6 | ELF64 loader | done |
| 6 | Userspace libc (crt0, malloc, printf, syscall wrappers) | done |
| 6 | userspace init (PID 1) + shell (cd/ls/cat/exec) | done |
| 7 | e1000 NIC driver (PCI detect, DMA TX/RX rings) | done |
| 7 | Ethernet layer (frame parser/builder, ethertype dispatch) | done |
| 7 | ARP (request/reply, table) | done |
| 7 | IPv4 (header, checksum, routing, dispatch) | done |
| 7 | ICMP (echo req/reply) | done |
| 7 | UDP (datagrams, port dispatch) | done |
| 7 | TCP (state machine, sliding window, retransmit) | done |
| 8 | SMP, TTY, HTTP server | pending |

## Roadmap

```
Bootstrap → Memory → Drivers → Storage/VFS → Processes → Scheduler
→ Syscalls → Userspace shell → TCP/IP stack → HTTP server
```

## Setup

> **Windows users:** all build tools run under WSL2. The Makefile calls `wsl` internally.
> See [docs/setup-wsl.md](docs/setup-wsl.md) for first-time setup.

- [docs/setup-wsl.md](docs/setup-wsl.md) — install WSL2, cross-compiler, QEMU, GRUB tools
- [docs/setup-linux.md](docs/setup-linux.md) — native Linux / CI setup
- [docs/build.md](docs/build.md) — all `make` targets explained
- [docs/debug.md](docs/debug.md) — GDB + QEMU remote debugging
- [docs/architecture.md](docs/architecture.md) — kernel design, module layout, coding rules

## Quick start

```bash
# 1. Install deps (Debian/Ubuntu/WSL2)
sudo apt update && sudo apt install -y \
    gcc-x86-64-linux-gnu nasm qemu-system-x86 \
    grub-pc-bin grub-common xorriso mtools

# 2. Build ISO
make iso

# 3. Run in QEMU
make run
```

Expected serial output (COM1 → stdout):
```
[INFO] [serial] COM1 ready
[INFO] [vga] VGA text mode 80x25 ready
...
[INFO] [process] process subsystem ready, idle pid=0
[INFO] [scheduler] round-robin scheduler ready
```

## Build targets

| Target | Description |
|--------|-------------|
| `make` | Build `build/namelessos.elf` |
| `make iso` | Wrap ELF in GRUB2 ISO → `build/namelessos.iso` |
| `make run` | Boot ISO in QEMU (serial → stdio) |
| `make debug` | QEMU + GDB stub on `:1234` |
| `make disk` | Create blank 100 MB `build/disk.img` |
| `make clean` | Remove `build/` |

## Repository layout

```
.
├── boot/               # NASM entry point (Multiboot2 header, long mode setup)
├── src/
│   ├── lib/            # types, string, printf — zero dependencies
│   ├── core/           # module_registry — init order
│   ├── serial/         # COM1 driver, klog()
│   ├── vga/            # VGA text mode, kprintf()
│   ├── gdt/            # Global Descriptor Table
│   ├── idt/            # Interrupt Descriptor Table + exception handlers
│   ├── pic/            # 8259A PIC, IRQ routing
│   ├── pmm/            # Physical memory manager
│   ├── vmm/            # Virtual memory manager
│   ├── heap/           # Slab allocator
│   ├── timer/          # PIT @ 1000 Hz
│   ├── keyboard/       # PS/2 keyboard
│   ├── pci/            # PCI bus enumeration
│   ├── ata/            # ATA PIO disk driver
│   ├── vfs/            # Virtual filesystem layer
│   ├── tmpfs/          # In-memory filesystem
│   ├── devfs/          # /dev virtual filesystem
│   ├── process/        # Process control blocks
│   ├── scheduler/      # Round-robin preemptive scheduler
│   ├── syscall/        # SYSCALL/SYSRET dispatch table
│   └── elf/            # ELF64 loader
├── prompts/            # Per-session build logs (PROMPT_N.md)
├── docs/               # Setup and architecture docs
├── kernel.ld           # Linker script (higher-half kernel @ 0xFFFFFFFF80000000)
├── grub.cfg            # GRUB2 boot config
└── Makefile
```

## License

MIT — see [LICENSE](LICENSE).

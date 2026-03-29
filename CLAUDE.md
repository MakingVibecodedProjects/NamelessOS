# NamelessOS — Claude Code Memory

## Project
Monolithic x86_64 kernel in C (C11), built modularly across sessions.
Target: QEMU + real bare metal. Bootloader: GRUB2 Multiboot2.
Higher half kernel at 0xFFFFFFFF80000000. No hosted libc anywhere in kernel/.

## Current Status
<!-- Claude updates this section at end of every session -->
- **Last session:** 2026-03-29
- **Last completed:** Phase 5 Step 1 — src/process/ (TCB, 64-slot table, kthread_create, idle pid=0, cpu_context_t)
- **Next task:** Phase 5 Step 2 — src/scheduler/ (preemptive round-robin, context_switch asm, kthread_create integration)
- **Known issues:** none
- **Build note:** `make run` boots via GRUB2 ISO (grub.cfg + grub-mkrescue). `make iso` builds the ISO. Direct QEMU `-kernel` does not work with Multiboot2 ELF; always use ISO path.
- **Critical compiler flag:** `-mno-sse -mno-sse2 -mno-avx` required — without it GCC emits `movaps` in kernel code which causes faults before SSE context save is set up.

## Architecture Rules (NEVER violate)
- Every subsystem = `src/MODULE/` with `MODULE.h` / `MODULE.c` / `MODULE_internal.h`
- Cross-module comms: public `.h` API only — never `#include` another module's `.c`
- Dependency order: `lib → serial/vga → gdt/idt/pic → pmm → vmm → heap → timer/keyboard → process → scheduler → syscall → ata/pci → vfs → net → elf`
- No hosted libc headers anywhere in `kernel/` or `src/`
- All log output via `klog(LEVEL, "[module] msg")` — never raw printf in modules
- `PANIC(fmt, ...)` = prints file/line/registers/stack dump + halts all CPUs
- Every public function prefixed by module name: `pmm_`, `vfs_`, `tcp_`, etc.
- Every module has `module_init()` and `module_dump()` — both registered in module_registry

## Subsystem Dependency Graph
```
[hardware/boot]
      ↓
[lib] (string, printf, types — zero deps)
      ↓
[serial] [vga]
      ↓
[gdt] → [idt] → [pic]
      ↓
[pmm] → [vmm] → [heap]
      ↓
[timer] → [keyboard] → [pci]
      ↓
[process] → [scheduler] → [syscall]
      ↓
[ata] → [vfs] → [fat32] [ext2] [tmpfs] [devfs] [initrd]
      ↓
[net: e1000 → ethernet → arp → ipv4 → icmp → udp → tcp → socket → dhcp]
      ↓
[elf] → [userspace / libc / programs]
```

## Full Feature Roadmap

### Phase 1 — Kernel Bootstrap
1. Scaffold: Makefile, kernel.ld, boot/entry.asm, src/lib/, src/core/module_registry
2. src/vga/ — VGA text mode 80x25, kprintf %s %d %x %p %c
3. src/serial/ — COM1, klog(LEVEL, fmt) with levels DEBUG/INFO/WARN/ERROR/PANIC
4. src/gdt/ — flat 64-bit GDT, TSS stub
5. src/idt/ — IDT, exception handlers, PANIC with full register dump
6. src/pic/ — remap IRQs 0-15 to vectors 32-47, irq_register(n, handler)

### Phase 2 — Memory
7. src/pmm/ — Multiboot2 memory map parser, bitmap frame allocator
8. src/vmm/ — 4-level page tables, higher half kernel, page fault handler
9. src/heap/ — slab allocator, kmalloc/kfree/krealloc

### Phase 3 — Drivers
10. src/timer/ — PIT IRQ0, tick counter, ksleep(ms), timer_register_callback()
11. src/keyboard/ — PS/2 IRQ1, scancode→ASCII, 256-byte circular buffer
12. src/pci/ — bus enumeration, pci_find_device(vendor, device)

### Phase 4 — Storage & Filesystem
13. src/ata/ — ATA PIO read/write sectors
14. src/vfs/ — abstract VFS: open/read/write/close/readdir/stat, filesystem_register()
15. src/fat32/ — FAT32 backend
16. src/ext2/ — ext2 backend
17. src/tmpfs/ — in-memory FS for /tmp
18. src/devfs/ — /dev/null /dev/zero /dev/tty /dev/sda
19. src/initrd/ — Multiboot2 module as early ramdisk

### Phase 5 — Process & Scheduling
20. src/process/ — TCB, kernel stacks, process table, pid allocator
21. src/scheduler/ — preemptive round-robin, full context switch (SSE), kthread_create/exit
22. src/syscall/ — SYSCALL/SYSRET dispatch table
    Syscalls: read write open close exit getpid fork execve waitpid mmap munmap brk

### Phase 6 — Userspace
23. src/vmm/ addition — per-process page tables, COW fork
24. src/elf/ — ELF64 loader
25. userspace/libc/ — crt0, syscall wrappers, malloc, printf, string.h
26. userspace/programs/init — PID 1
27. userspace/programs/shell — cd ls cat exec

### Phase 7 — Networking
28. src/net/e1000/ — Intel e1000, PCI detect, DMA TX/RX rings
29. src/net/ethernet/ — frame parse/build, ethertype dispatch
30. src/net/arp/ — request/reply, ARP table
31. src/net/ipv4/ — headers, routing table
32. src/net/icmp/ — echo req/reply (ping)
33. src/net/udp/ — datagrams
34. src/net/tcp/ — full state machine, sliding window, retransmit
35. src/net/socket/ — BSD socket syscalls
36. src/net/dhcp/ — DHCP client, obtain IP on boot

### Phase 8 — Advanced
37. src/smp/ — APIC SIPI, per-CPU data (gs-based), spinlocks
38. src/tty/ — line discipline, /dev/tty0
39. src/dynlink/ — shared .so, PLT/GOT
40. userspace/programs/httpd — minimal HTTP/1.0 server

## Build Commands
```bash
make            # build kernel ELF → build/namelessos.elf
make run        # boot in QEMU
make debug      # QEMU + GDB stub on :1234
make userspace  # cross-compile all userspace ELFs
make disk       # create disk.img FAT32 + userspace programs
make clean
```

## Full QEMU Command
```bash
qemu-system-x86_64 \
  -cdrom build/namelessos.iso \
  -m 512M \
  -serial stdio \
  -netdev user,id=net0 -device e1000,netdev=net0 \
  -drive file=disk.img,format=raw,if=ide \
  -display none \
  -boot d
```

## Cross-Compiler Flags
- **Kernel:** `-ffreestanding -nostdlib -nostdinc -mno-red-zone -mcmodel=kernel -fno-pic -fno-pie -mno-sse -mno-sse2 -mno-avx -O2 -Wall -Wextra -std=c11`
- **Userspace:** `-nostdlib -O2`
- **Assembler:** NASM for `.asm` files

## Session Start Protocol (EVERY session, no exceptions)
1. Run `find src/ -name "*.h" | sort` — read ALL public headers
2. Read `src/core/module_registry.c` — see which modules are registered
3. Read "Current Status" above — know exactly where we left off
4. Run `/status` for a quick overview
5. Implement ONE complete module at a time
6. After each module: show QEMU test command + expected serial output
7. NEVER break existing modules — state refactors explicitly first

## Session End Protocol (EVERY session, automatically — never wait to be asked)
When the session is wrapping up or a natural stopping point is reached, Claude
automatically does BOTH of these without being prompted:
1. Read ALL existing `prompts/PROMPT_*.md` files for style reference, then write
   `prompts/PROMPT_N.md` — N is highest existing PROMPT number + 1.
   Match the exact style of prior prompts: header, what was built, key decisions,
   verified serial output, next session prompt section.
2. Update "Current Status" in this CLAUDE.md with what was done and what's next

## Coding Style
- Every public API function gets a one-line doc comment in the `.h`
- Magic numbers = named constants in `MODULE_internal.h`
- Structs: `typedef struct { ... } name_t;`
- Error returns: 0 = success, negative errno = failure
- Always NULL-check pointers before dereferencing

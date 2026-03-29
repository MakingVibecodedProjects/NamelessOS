# NamelessOS — Claude Code Session Context

## Project
Monolithic x86_64 kernel in C (C11), built modularly across sessions.
Target: QEMU + real bare metal. Bootloader: GRUB2 Multiboot2.
Higher half kernel at `0xFFFFFFFF80000000`. No hosted libc anywhere in `src/`.

> Build, QEMU, and debug details live in [docs/build.md](docs/build.md),
> [docs/debug.md](docs/debug.md), and [docs/setup-wsl.md](docs/setup-wsl.md).

---

## Current Status

- **Last session:** 2026-03-29
- **Last completed:** Phase 5 Step 1 — `src/process/` (TCB, 64-slot table, `kthread_create`, idle pid=0, `cpu_context_t`)
- **Next task:** Phase 5 Step 2 — `src/scheduler/` (preemptive round-robin, `context_switch` asm, timer-driven tick)
- **Known issues:** none
- **Build note:** Always `make iso` then `make run`. Direct `-kernel` QEMU flag does not work with Multiboot2.
- **Platform:** build tools run under WSL2 on Windows. Use `wsl make iso && wsl make run` from PowerShell, or open a WSL terminal.

---

## Architecture Rules (NEVER violate)

- Every subsystem = `src/MODULE/` with `MODULE.h` / `MODULE.c` / `MODULE_internal.h`
- Cross-module comms: public `.h` API only — never `#include` another module's `.c`
- No hosted libc headers anywhere in `src/` — not even `<stdint.h>`
- All log output via `klog(LEVEL, "[module] msg")` — never `printf` in modules
- `PANIC(fmt, ...)` = prints file/line/registers/stack dump + halts all CPUs
- Every public function prefixed by module name: `pmm_`, `vfs_`, `tcp_`, etc.
- Every module has `module_init()` and `module_dump()` — both registered in `module_registry`
- Zero warnings policy: build must emit **no warnings** at `-Wall -Wextra`

---

## Dependency Order

```
lib → serial/vga → gdt/idt/pic → pmm → vmm → heap
    → timer → keyboard → pci
    → ata → vfs → tmpfs → devfs
    → process → scheduler → syscall
    → elf → userspace
    → net (e1000 → ethernet → arp → ipv4 → icmp → udp → tcp → socket → dhcp)
```

Module registration order in `src/core/module_registry.c` must match this graph.

---

## Full Feature Roadmap

### Phase 1 — Kernel Bootstrap ✓
1. Scaffold: Makefile, kernel.ld, boot/entry.asm, src/lib/, src/core/module_registry
2. `src/vga/` — VGA text mode 80×25, `kprintf` `%s %d %x %p %c`
3. `src/serial/` — COM1, `klog(LEVEL, fmt)` with DEBUG/INFO/WARN/ERROR/PANIC
4. `src/gdt/` — flat 64-bit GDT, TSS stub
5. `src/idt/` — IDT, exception handlers, PANIC with full register dump
6. `src/pic/` — remap IRQs 0–15 to vectors 32–47, `irq_register(n, handler)`

### Phase 2 — Memory ✓
7. `src/pmm/` — Multiboot2 memory map parser, bitmap frame allocator
8. `src/vmm/` — 4-level page tables, higher-half kernel, page fault handler
9. `src/heap/` — slab allocator, `kmalloc` / `kfree` / `krealloc`

### Phase 3 — Drivers ✓
10. `src/timer/` — PIT IRQ0, tick counter, `ksleep(ms)`, `timer_register_callback()`
11. `src/keyboard/` — PS/2 IRQ1, scancode→ASCII, 256-byte circular buffer
12. `src/pci/` — bus enumeration, `pci_find_device(vendor, device)`

### Phase 4 — Storage & Filesystem ✓
13. `src/ata/` — ATA PIO read/write sectors
14. `src/vfs/` — abstract VFS: open/read/write/close/readdir/stat, `filesystem_register()`
15. `src/tmpfs/` — in-memory FS for `/tmp`
16. `src/devfs/` — `/dev/null` `/dev/zero` `/dev/tty` `/dev/sda`

### Phase 5 — Process & Scheduling ← current
17. `src/process/` ✓ — TCB, kernel stacks, process table, pid allocator
18. `src/scheduler/` — preemptive round-robin, `context_switch` asm, `kthread_create`/exit
19. `src/syscall/` — SYSCALL/SYSRET dispatch table
    Syscalls: read write open close exit getpid fork execve waitpid mmap munmap brk

### Phase 6 — Userspace
20. `src/vmm/` addition — per-process page tables, COW fork
21. `src/elf/` — ELF64 loader
22. `userspace/libc/` — crt0, syscall wrappers, malloc, printf, string.h
23. `userspace/programs/init` — PID 1
24. `userspace/programs/shell` — cd ls cat exec

### Phase 7 — Networking
25. `src/net/e1000/` — Intel e1000, PCI detect, DMA TX/RX rings
26. `src/net/ethernet/` — frame parse/build, ethertype dispatch
27. `src/net/arp/` — request/reply, ARP table
28. `src/net/ipv4/` — headers, routing table
29. `src/net/icmp/` — echo req/reply (ping)
30. `src/net/udp/` — datagrams
31. `src/net/tcp/` — full state machine, sliding window, retransmit
32. `src/net/socket/` — BSD socket syscalls
33. `src/net/dhcp/` — DHCP client, obtain IP on boot

### Phase 8 — Advanced
34. `src/smp/` — APIC SIPI, per-CPU data (gs-based), spinlocks
35. `src/tty/` — line discipline, `/dev/tty0`
36. `src/dynlink/` — shared `.so`, PLT/GOT
37. `userspace/programs/httpd` — minimal HTTP/1.0 server

---

## Session Start Protocol (EVERY session, no exceptions)

1. Read `CLAUDE.md` **Current Status** — know exactly where we left off
2. Run `find src/ -name "*.h" | sort` — scan all public headers
3. Read `src/core/module_registry.c` — verify module registration order
4. Implement ONE complete module at a time
5. After each module: show expected serial output, build and verify zero warnings
6. NEVER break existing modules — state any refactors explicitly before making them

## Session End Protocol (EVERY session, automatically — never wait to be asked)

1. Read ALL existing `prompts/PROMPT_*.md` for style reference
2. Write `prompts/PROMPT_N.md` (N = highest + 1) matching the exact style with nav links:
   - Top and bottom bar: `[← N-1](PROMPT_N-1.md) | [index](README.md) | **N** | [N+1 →](PROMPT_N+1.md)`
   - Update `prompts/README.md` table — add the new row, mark previous as done
   - Update the previous prompt's `15 →` placeholder to a real link `[15 →](PROMPT_15.md)`
3. Update **Current Status** in this file (`CLAUDE.md`)
4. Update `README.md` — "What's built so far" table: mark completed modules as done, update in-progress row
5. Update `docs/architecture.md` if new modules, layers, or rules were added
6. Git commit — one commit per completed module:
   `feat(phaseN): module — one-line summary`
   **No Co-Authored-By trailers** — single author only
7. `git push origin master` after every session
8. Never batch unrelated modules into one commit

---

## Git Rules

- Commit after EVERY completed module — never accumulate
- Format: `feat(phaseN): module — description`
  Example: `feat(phase3): timer — PIT 1000Hz IRQ0`
- Always `git status` before committing — verify what will be staged
- Never commit `build/`, `.claude/`, `.vscode/` (covered by .gitignore)
- After each commit: `git log --oneline -5` to confirm
- Remote: `https://github.com/MakingVibecodedProjects/NamelessOS`
- Push after each session end: `git push origin master`

---

## Coding Style

- Every public API function gets a one-line doc comment in the `.h`
- Magic numbers = named constants in `MODULE_internal.h`
- Structs: `typedef struct { ... } name_t;`
- Error returns: 0 = success, negative = failure
- Always NULL-check pointers before dereferencing
- Prefer explicit casts over implicit conversions

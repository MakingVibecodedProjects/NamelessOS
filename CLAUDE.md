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
- **Last completed:** Phase 6 Step 2 — ELF64 loader (`src/elf/`: `elf_load`, PT_LOAD mapper, PTE flags from p_flags)
- **Next task:** Phase 6 Step 3 — userspace libc (`userspace/libc/`: crt0, syscall wrappers, malloc, printf, string.h)
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
4. If **Current Status** says "in progress" or "WIP" — read the partial files before touching anything
5. Implement ONE complete module at a time
6. After each module: `wsl make iso` → `wsl make run` → confirm serial output matches expected
7. NEVER break existing modules — state any refactors explicitly before making them

## Session End Protocol (EVERY session, automatically — never wait to be asked)

1. Read ALL existing `prompts/PROMPT_*.md` for style reference
2. Write `prompts/PROMPT_N.md` (N = highest + 1) matching the exact style with nav links:
   - Top and bottom bar: `[← N-1](PROMPT_N-1.md) | [index](README.md) | **N** | [N+1 →](PROMPT_N+1.md)`
   - Update `prompts/README.md` table — add the new row, mark previous as done
   - Update the previous prompt's nav placeholder AND its `## Next session` body line to real links
     - Nav bar: `N →` → `[N →](PROMPT_N.md)` (both top and bottom)
     - Body: `PROMPT_N — ...` → `[PROMPT_N →](PROMPT_N.md) — ...`
   - **PROMPT_N is a completion log, not a start prompt** — write it AFTER the module is built and verified, never before
3. Update **Current Status** in this file (`CLAUDE.md`)
4. Update `README.md` — "What's built so far" table: mark completed modules as done, update in-progress row
5. Update `docs/architecture.md` if new modules, layers, or rules were added
6. Append any new bugs, surprises, or non-obvious behaviours discovered this session to the **Known Traps** section — one bullet per item, under the relevant category. If it burned time or would have been non-obvious to a fresh Claude, it belongs here.
7. Git commit — one commit per completed module:
   `feat(phaseN): module — one-line summary`
   **No Co-Authored-By trailers** — single author only
7. `git push origin master` after every session
8. Never batch unrelated modules into one commit

---

## Build & Verification

A module is only "complete" when ALL of these pass:
1. `wsl make iso` — zero warnings, zero errors
2. `wsl make run` — QEMU boots, no triple fault, no hang
3. Serial output contains the expected `[INFO] [module] ...` line
4. No existing module's output disappeared or changed

**When the build fails:**
- Read the full error — never guess, never blindly change flags
- Compiler errors: fix the source; never suppress with casts unless the cast is semantically correct
- Linker errors: check `src/core/module_registry.c` includes and the new module's `extern` declarations
- QEMU triple fault: attach GDB (`wsl make debug`), break at `_start`, step through init

**WSL clock skew** (Windows only): if `make` says everything is up to date after a crash, run `touch src/**/*.c` or `wsl make clean && wsl make iso`

---

## Known Traps (learned the hard way)

### Compiler / build
- **`-mno-sse -mno-sse2 -mno-avx` is mandatory** — GCC emits `movaps` for struct copies without it; causes #GP before SSE context is saved
- **`kprintf` has no width padding** — `%02x` silently prints wrong output; use plain `%x`
- **Zero warnings is a hard rule** — `__attribute__((unused))` is acceptable for intentionally-unused static helpers (e.g. `tmpfs_create`); never suppress real warnings with casts
- **`find src/ -name "*.asm"` is picked up automatically** — the Makefile glob catches all `.asm` under `src/`; no manual rule needed for new modules
- **`.note.GNU-stack` must go at the END of NASM `.asm` files** — placing `section .note.GNU-stack` before `global` causes the linker to put the symbol inside that section and then discard it, breaking the link; always append it after all code

### QEMU / hardware
- **QEMU disk layout** — `-cdrom` ISO takes primary master (0x1F0); ATA disk must go on secondary (0x170) via `index=1`; probe both channels in the driver
- **ATAPI detection order** — send `IDENTIFY` first, *then* check mid/hi bytes (0x14/0xEB); at reset time mid/hi=0 for everything so the signature check is useless before IDENTIFY
- **`poll_drq` must skip BSY** — `if (s & ATA_SR_BSY) continue` before testing DRQ; without it the loop times out immediately
- **WSL2 clock skew** — `make` may think everything is up to date after a crash due to future timestamps on Windows; fix: `touch src/**/*.c` or `make clean && make iso`
- **QEMU `-kernel` does not work** — Multiboot2 requires booting via GRUB ISO; `-kernel` bypasses the MB2 info struct setup

### VMM / address spaces
- **`vmm_switch_to` must skip the write if CR3 is unchanged** — writing the same value to CR3 flushes the TLB unnecessarily; always compare before writing
- **`vmm_map_user_page` must only `invlpg` when the target PML4 is loaded** — firing `invlpg` for a different address space has no effect and would be confusing; gate it on `read_cr3() == target_pml4_phys`
- **COW fault check: `err & 3 == 3` means PRESENT+WRITE** — the error code bit layout is bit0=P, bit1=W/R, bit2=U/S; a COW fault is always a write to a present page (both bits set); non-present faults are allocation, not COW
- **`vmm_fork_pml4` must reload parent's CR3 after marking PTEs read-only** — after clearing PTE_WRITE and setting PTE_COW in the parent's PTs, the TLB may still have the old writable entries cached; `write_cr3(src_pml4_phys)` flushes them

### Syscall
- **`o64 sysret` is mandatory in NASM** — without the `o64` prefix NASM emits the 32-bit SYSRET variant, which returns to 32-bit compatibility mode instead of 64-bit long mode; the CPU immediately triple-faults
- **STAR[63:48] is `user_base`, not `user_CS`** — SYSRET64 sets CS = `STAR[63:48]+16 | 3` and SS = `STAR[63:48]+8 | 3`; so `STAR[63:48]` must be `user_data_selector - 8`, not the user code selector; layout must be `..., user_data, user_code, TSS` in the GDT for this arithmetic to work
- **User GDT segments must sit before TSS** — adding DPL=3 descriptors shifts TSS from index 3 to index 5; `SEG_TSS` and all `ltr` / TSS references must be updated together
- **rcx holds saved user RIP on SYSCALL entry — do not clobber before pushing** — the arg shuffle (rdi←rax, …, rcx←rdx) must be preceded by `push rcx` or the user return address is lost and SYSRET jumps to garbage

### Scheduler / process
- **`context_switch` is one-way per call** — execution resumes in the *new* process's stack frame; code after `context_switch()` in the old process only runs when that process is switched back in
- **Idle process (pid 0) `kstack = NULL`** — it uses the original boot stack; scheduler must never `kfree` it or touch `kstack`
- **`process_set_current()` must be called before `context_switch()`** — not after; the new thread sees itself as current from its very first instruction
- **`rsp = stack_top - 8` for new threads** — leaves an 8-byte slot at the top; `context_switch` jumps to `rip` directly (no `call`/`ret`), so a fresh thread starts with RSP already pointing below the sentinel zero

### Git / GitHub
- **`filter-branch` requires a clean working tree** — always commit or stash before rewriting history
- **No `Co-Authored-By` trailers** — single author only; strip them with `--msg-filter 'sed "/^Co-Authored-By:.*/d"'`
- **`git reset --soft <hash>` + `git reset HEAD -- .`** — the two-step to unstage everything after a soft reset so you can recommit files individually
- **GitHub "authored and committed" duplication** — see Git Rules for the correct identity config

---

## Files That Need Extra Care

These files underpin everything — never modify without explicit discussion:

| File | Why it's fragile |
|------|-----------------|
| `boot/entry.asm` | Sets up page tables, long mode, passes MB2 ptr — wrong change = no boot |
| `kernel.ld` | Section order and alignment determine physical layout — wrong = page fault at boot |
| `src/lib/types.h` | Every module depends on it — changing a typedef breaks everything |
| `src/lib/module.h` | `kernel_module_t` layout must stay stable — all modules embed it |
| `src/core/module_registry.c` | Init order is the dependency graph — wrong order = use-before-init crash |

---

## Git Rules

- Commit after EVERY completed module — never accumulate
- Format: `feat(phaseN): module — description`
  Example: `feat(phase3): timer — PIT 1000Hz IRQ0`
- Always `git status` before committing — verify what will be staged
- Never commit `build/`, `.claude/`, `.vscode/` (covered by .gitignore)
- After each commit: `git log --oneline -5` to confirm
- Remote: `https://github.com/MakingVibecodedProjects/NamelessOS`
- Push after every session: `git push origin master`
- Force push only after history rewrites: `git push --force origin master`


---

## Coding Style

- Every public API function gets a one-line doc comment in the `.h`
- Magic numbers = named constants in `MODULE_internal.h`
- Structs: `typedef struct { ... } name_t;`
- Error returns: 0 = success, negative = failure
- Always NULL-check pointers before dereferencing
- Prefer explicit casts over implicit conversions

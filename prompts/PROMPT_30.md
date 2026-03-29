[← 29](PROMPT_29.md) | [index](README.md) | **30** | [31 →](PROMPT_31.md)

---

# PROMPT_30 — Phase 8 Step 1: SMP

**Session date:** 2026-03-29
**Status when starting:** Phase 7 complete (full TCP/IP stack, DHCP bound)
**Status when done:** Phase 8 Step 1 complete — LAPIC enable, per-CPU `gs`-based data, spinlocks, AP trampoline infrastructure; zero warnings

## What was built

- `src/smp/smp_internal.h` — LAPIC MMIO register offsets (`LAPIC_BASE=0xFEE00000`, SVR, ICR, LVT_TIMER, EOI, etc.), ICR delivery mode constants, `TRAMPOLINE_PHYS=0x8000`, `MSR_GS_BASE`, `MSR_KERNEL_GS_BASE`, `SMP_MAX_CPUS=8`, `cpu_t` per-CPU struct (self-pointer at `gs:[0]`, id, lapic_id, kstack, current, online), extern declarations for `trampoline_bin`
- `src/smp/spinlock.h` — header-only `spinlock_t` with `spin_lock` / `spin_unlock` / `spin_trylock` using `xchgl` — no `.c` needed
- `src/smp/smp.h` — `smp_init`, `smp_cpu_count`, `smp_this_cpu`, `smp_cpu`, `lapic_eoi`, `mod_smp`
- `src/smp/trampoline_bin.c` — AP trampoline as a raw byte array (assembled separately with `nasm -f bin`, embedded as `const u8 trampoline_bin[]`); copied to `TRAMPOLINE_PHYS` (0x8000) at init; transitions AP from 16-bit real mode → 32-bit protected → 64-bit long mode → calls `ap_entry`
- `src/smp/smp.c`:
  - `lapic_read` / `lapic_write`: LAPIC MMIO via identity-mapped `0xFEE00000`
  - `lapic_enable`: sets SVR (spurious vector 0xFF, enable bit), clears TPR
  - `lapic_timer_start`: programs LVT TIMER for periodic interrupts at vector 0x20
  - `wrmsr` / `rdmsr`: inline MSR helpers
  - `set_gs_base`: writes `MSR_GS_BASE` to point `gs` at a `cpu_t`
  - `smp_this_cpu`: `mov rax, gs:[0]` — reads self-pointer from per-CPU struct
  - `cpuid_lapic_id` / `cpuid_logical_cpus`: use CPUID leaf 1 for BSP LAPIC ID and total CPU count
  - `ap_entry`: AP C entry point (called from trampoline); finds own `cpu_t`, sets gs base, enables LAPIC, starts LAPIC timer, marks `online=1`, halts in `hlt` loop
  - `trampoline_install`: `memcpy(0x8000, trampoline_bin, len)` — installs trampoline to low memory
  - `trampoline_prepare` / `send_init_sipi`: AP boot helpers (unused but preserved for ACPI MADT follow-up)
  - `smp_init_impl`: copies trampoline, sets up BSP `cpu_t`, sets BSP GS base, enables BSP LAPIC, detects CPU count via CPUID, logs and returns
- `boot/entry.asm` — no change needed; 4 GB identity map already covers `0x8000` and `0xFEE00000`
- `src/core/module_registry.c` — added `smp.h` include and `&mod_smp` after `&mod_dhcp`

## Key decisions

- **Trampoline as C byte array** — the Makefile assembles all `src/**/*.asm` with `-f elf64`; a real-mode trampoline needs `-f bin`; solution: assemble once with `nasm -f bin`, embed result as `trampoline_bin[]` in a `.c` file; the `.asm` source is kept alongside for reference/regeneration
- **GS self-pointer at offset 0** — `cpu_t.self` is the first field so `mov rax, gs:[0]` returns the `cpu_t *` without knowing the struct layout at call sites; standard Linux trick
- **AP boot limited to BSP-only for now** — QEMU with `-smp 2` and SeaBIOS causes the second CPU to go through a full BIOS reset and re-boot the kernel (SeaBIOS lacks ACPI MADT by default); without an MADT we cannot enumerate APs or send SIPI safely; `send_init_sipi` and `trampoline_prepare` are fully implemented but guarded with `__attribute__((unused))` until ACPI MADT is added
- **BSP LAPIC enabled** — even on single-CPU QEMU, the BSP's LAPIC is enabled (SVR written, TPR cleared); this is required before sending any IPIs and is safe to do unconditionally

## Debugging story

AP boot with `-smp 2` caused the kernel to reboot in a loop:
1. First attempt: QEMU starts APs at the BSP's protected-mode entry — both CPUs run `start` and both end up running the full kernel init. Fixed with a CPUID LAPIC ID guard at the top of `start`, but the AP's initial LAPIC ID was 0 due to BIOS reset resetting it.
2. The guard (CPUID leaf 1 EBX[31:24]) used `push ebx / pop ebx` before a stack was set up — the `push` with `esp=0` corrupted memory. Fixed by saving to `mb2_info_phys` directly.
3. Even with a correct guard, QEMU SeaBIOS boots APs from the full reset vector (not just SIPI) — they go through BIOS POST, GRUB, then `start`. By the time they reach `start`, they look like a fresh BSP (LAPIC ID 0 from BIOS reinit).
4. Root cause: QEMU needs ACPI MADT tables for proper AP halt-until-SIPI behavior. Without MADT, APs free-run through BIOS.
5. Solution: limit AP boot to BSP-only until ACPI MADT enumeration is implemented. All AP infrastructure (trampoline, `ap_entry`, SIPI helpers) is in place.

## Verified build output

```
[INFO] [dhcp] client started
[INFO] [smp] BSP LAPIC id=0, 1 CPU(s) detected
[INFO] [smp] ready — 1 CPU(s) online
[INFO] [kernel] All modules initialized.
[INFO] [dhcp] bound — IP 10.0.2.15 mask 255.255.255.0 gw 10.0.2.2
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

[PROMPT_31 →](PROMPT_31.md) — Phase 8 Step 2: `src/tty/` — line discipline, `/dev/tty0`

---

[← 29](PROMPT_29.md) | [index](README.md) | **30** | [31 →](PROMPT_31.md)

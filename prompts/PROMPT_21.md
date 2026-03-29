[← 20](PROMPT_20.md) | [index](README.md) | **21** | [22 →](PROMPT_22.md)

---

# PROMPT_21 — Phase 7 Step 1: e1000 NIC driver

**Session date:** 2026-03-29
**Status when starting:** Phase 6 Step 4 complete (init + shell ELFs link clean)
**Status when done:** Phase 7 Step 1 complete — e1000 driver initialises, MAC read, TX/RX rings armed; zero warnings

## What was built

- `src/net/e1000/e1000_internal.h` — private constants: PCI vendor/device IDs, MMIO register offsets (CTRL, EERD, ICR, IMS, IMC, RCTL, TCTL, TIPG, RDBAL/RDBAH/RDLEN/RDH/RDT, TDBAL etc.), CTRL/RCTL/TCTL/EERD/RAH flag bits, ring sizes (32 RX + 32 TX descriptors), packed `e1000_rx_desc_t` and `e1000_tx_desc_t` structs (16 bytes each), `e1000_dev_t` driver state
- `src/net/e1000/e1000.h` — public API: `e1000_init`, `e1000_send(buf, len)`, `e1000_register_rx_callback(cb)`, `e1000_get_mac(mac[6])`, `e1000_poll()`, `e1000_rx_cb_t` callback type, `mod_e1000`
- `src/net/e1000/e1000.c` — full driver:
  - `e1000_init_impl`: PCI detect, BAR0 MMIO pointer, bus-master enable, software reset (spin on CTRL_RST clear), SLU+ASDE, clear MTA, read MAC (EEPROM first; RAL0/RAH0 fallback for QEMU), program RAL0/RAH0, mask all interrupts, init RX ring, init TX ring
  - `init_rx`: allocate 16-byte-aligned descriptor ring + 32 × 2 KB receive buffers via `kzalloc`; program RDBAL/RDBAH/RDLEN/RDH/RDT; enable RCTL (EN | BAM | BSIZE_2K | SECRC | UPE | MPE)
  - `init_tx`: allocate descriptor ring + 32 × 2 KB transmit buffers; pre-mark all TX descriptors DD (free); program TDBAL/TDBAH/TDLEN/TDH/TDT; enable TCTL (EN | PSP | CT=15 | COLD=63); set TIPG = IPGT=10 IPGR1=8 IPGR2=6
  - `e1000_send`: find free TX descriptor (spin on DD bit), copy frame, set EOP|FCS|RS, advance TDT tail
  - `e1000_poll`: walk RX ring checking DD+EOP, deliver heap copy to callback, return descriptor to hardware, advance RDT tail
  - Polled (no interrupt) mode: upper layers call `e1000_poll()` periodically
- `src/core/module_registry.c` — added `#include "../net/e1000/e1000.h"` and `&mod_e1000` registration (last entry, after `mod_elf`)
- `src/pmm/pmm.c` — **bug fix**: changed `mark_range_used(kstart_phys, ...)` to `mark_range_used(0x100000, kend_phys - 0x100000)` so the boot section (entry code + page tables at physical 0x100000–0x103FFF) is excluded from the free pool; previously the slab allocator could recycle those frames and zero the PML4
- `src/pmm/pmm_internal.h` — removed unused `__kernel_start[]` declaration (no longer needed after the PMM fix)

## Key decisions

- **No MMIO re-mapping** — QEMU places the e1000 MMIO BAR at `0xfeb80000`, which falls within the boot identity-mapped 4th 1 GB page (`pdpt_id[3]` covers 0xC0000000–0xFFFFFFFF); direct pointer access works without additional `vmm_map_page` calls
- **EEPROM → RAL/RAH fallback** — QEMU's 82540EM emulation does not reliably respond to EERD; after reset the hardware pre-populates RAL0/RAH0 with the virtual MAC (`52:54:00:12:34:56`); read from there if EEPROM word 0 returns 0
- **Polled mode** — all IRQs masked via IMC; upper layers call `e1000_poll()` to drain the RX ring; avoids wiring a PIC IRQ handler in this phase
- **PMM boot-section fix** — the root crash was the slab allocator zeroing a 4 KB frame at physical `0x101000` which is the PML4 page built by entry.asm; the fix is to mark the entire range `0x100000–__kernel_end_phys` used, not just from `__kernel_start`

## Verified build output

```
[INFO] [e1000] found at PCI 0:3.0 BAR0=0xfeb80000
[INFO] [e1000] ready — MAC 52:54:0:12:34:56
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

[PROMPT_22 →](PROMPT_22.md) — Phase 7 Step 2: `src/net/ethernet/` — Ethernet frame parser/builder, ethertype dispatch.

---

[← 20](PROMPT_20.md) | [index](README.md) | **21** | [22 →](PROMPT_22.md)

[← 24](PROMPT_24.md) | [index](README.md) | **25** | [26 →](PROMPT_26.md)

---

# PROMPT_25 — Phase 7 Step 5: ICMP

**Session date:** 2026-03-29
**Status when starting:** Phase 7 Step 4 complete (IPv4 header parser, checksum, routing, protocol dispatch)
**Status when done:** Phase 7 Step 5 complete — ICMP echo request/reply (ping); zero warnings

## What was built

- `src/net/icmp/icmp_internal.h` — type constants (`ICMP_TYPE_ECHO_REQUEST=8`, `ICMP_TYPE_ECHO_REPLY=0`), packed `icmp_hdr_t` (8 bytes: type, code, checksum, id, seq), `ICMP_HDR_LEN=8`
- `src/net/icmp/icmp.h` — public API: `icmp_init`, `icmp_ping(dst_ip, id, seq, data, len)`, `mod_icmp`
- `src/net/icmp/icmp.c` — full implementation:
  - `icmp_rx`: registered as IPv4 ICMP handler; handles echo request (type 8, code 0) — `memcpy` payload into reply buffer, set type=0, zero and recompute checksum via `ip_checksum`, call `ipv4_send(src_ip, IPPROTO_ICMP, ...)`; logs id and seq
  - `icmp_ping`: builds echo request packet (fill header fields in network byte order, compute checksum), calls `ipv4_send`
  - `icmp_init_impl`: calls `ipv4_register(IPPROTO_ICMP, icmp_rx)`
- `src/core/module_registry.c` — added `icmp.h` include and `&mod_icmp` after `&mod_ipv4`

## Key decisions

- **`ip_checksum` reused directly** — ICMP checksum covers the entire ICMP message (header + data); `ip_checksum` from `ipv4_internal.h` is the standard Internet checksum and works without modification
- **`htons`/`ntohs` via `ethernet_internal.h`** — these helpers are defined as inlines there; icmp.c includes that header rather than duplicating the definitions
- **Reply reuses full payload** — the `memcpy` + type-swap approach correctly handles variable-length echo payloads without knowing anything about the payload content

## Bug fixed during integration

- `ntohs`/`htons` caused `implicit declaration` warnings and linker errors — added `#include "../ethernet/ethernet_internal.h"` to icmp.c to pull in the inline definitions

## Verified build output

```
[INFO] [ipv4] ready
[INFO] [icmp] ready
[INFO] [kernel] All modules initialized.
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

[PROMPT_26 →](PROMPT_26.md) — Phase 7 Step 6: `src/net/udp/` — UDP datagrams.

---

[← 24](PROMPT_24.md) | [index](README.md) | **25** | [26 →](PROMPT_26.md)

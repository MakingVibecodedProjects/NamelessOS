[← 23](PROMPT_23.md) | [index](README.md) | **24** | [25 →](PROMPT_25.md)

---

# PROMPT_24 — Phase 7 Step 4: IPv4

**Session date:** 2026-03-29
**Status when starting:** Phase 7 Step 3 complete (ARP ready, 16-entry table)
**Status when done:** Phase 7 Step 4 complete — IPv4 header parser, checksum, routing, protocol dispatch; zero warnings

## What was built

- `src/net/ipv4/ipv4_internal.h` — protocol number constants (ICMP=1, TCP=6, UDP=17), packed `ip_hdr_t` (20 bytes, no options), field constants (VER_IHL=0x45, DEFAULT_TTL=64, DF flag), inline `ip_checksum(data, len)` (Internet checksum over any buffer), `IPV4_MAX_HANDLERS=8`
- `src/net/ipv4/ipv4.h` — public API: `IPPROTO_*` constants, `ipv4_rx_handler_t` callback type (`payload`, `len`, `src_ip`, `dst_ip` — all host byte order), `ipv4_init`, `ipv4_register(proto, handler)`, `ipv4_set_addr(ip, netmask, gateway)`, `ipv4_get_addr()`, `ipv4_send(dst_ip, proto, payload, len)`, `mod_ipv4`
- `src/net/ipv4/ipv4.c` — full implementation:
  - `ipv4_rx`: validate version/IHL/total_len, drop fragments (non-zero offset or MF flag), accept unicast-to-us/broadcast/unconfigured, dispatch to registered protocol handler
  - `ipv4_register`: append proto+handler to table
  - `ipv4_set_addr`: store ip/netmask/gateway, call `arp_set_ip` to sync ARP layer
  - `ipv4_send`: on-link vs off-link routing (gateway for off-subnet), ARP lookup (send request + drop if miss), build 20-byte IP header with checksum, call `ethernet_send`
  - `pkt_id` counter for IP identification field
- `src/net/arp/arp.h` — added `arp_set_ip(u32 ip)` declaration
- `src/net/arp/arp.c` — added `arp_set_ip` implementation (one-liner: `our_ip = ip`)
- `src/core/module_registry.c` — added `ipv4.h` include and `&mod_ipv4` after `&mod_arp`

## Key decisions

- **Fragment drop, no reassembly** — full IP reassembly adds significant complexity for little gain at this stage; all packets we generate have DF set; incoming fragments are silently dropped
- **ARP-miss → drop** — on ARP cache miss, `ipv4_send` fires an ARP request and returns -1; the caller retries on next timer tick; this is standard for simple stacks and avoids a pending-packet queue
- **`ip_checksum` as inline in the internal header** — reused by ICMP (pseudo-header) and TCP/UDP (pseudo-header) in later phases without a separate compilation unit
- **`arp_set_ip` added to ARP** — IPv4 sets its address; ARP needs to know it to respond to who-has; the function is a one-liner that keeps the coupling explicit and avoids reaching into ARP internals from IPv4

## Verified build output

```
[INFO] [arp] ready (16-entry table)
[INFO] [ipv4] ready
[INFO] [kernel] All modules initialized.
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

[PROMPT_25 →](PROMPT_25.md) — Phase 7 Step 5: `src/net/icmp/` — ICMP echo request/reply (ping).

---

[← 23](PROMPT_23.md) | [index](README.md) | **24** | [25 →](PROMPT_25.md)

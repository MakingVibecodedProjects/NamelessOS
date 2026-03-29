[← 25](PROMPT_25.md) | [index](README.md) | **26** | [27 →](PROMPT_27.md)

---

# PROMPT_26 — Phase 7 Step 6: UDP

**Session date:** 2026-03-29
**Status when starting:** Phase 7 Step 5 complete (ICMP echo request/reply)
**Status when done:** Phase 7 Step 6 complete — UDP datagrams, port dispatch, RFC-compliant checksum; zero warnings

## What was built

- `src/net/udp/udp_internal.h` — packed `udp_hdr_t` (8 bytes: src_port, dst_port, length, checksum), `udp_pseudo_hdr_t` (12 bytes for checksum calculation), `UDP_HDR_LEN=8`, `UDP_MAX_HANDLERS=16`
- `src/net/udp/udp.h` — public API: `udp_rx_handler_t` callback type (data, len, src_ip, src_port, dst_port), `udp_init`, `udp_register(dst_port, handler)` (port=0 = catch-all), `udp_send(dst_ip, src_port, dst_port, data, len)`, `mod_udp`
- `src/net/udp/udp.c` — full implementation:
  - `csum_add(sum, buf, len)`: byte-safe checksum accumulator — iterates via `u8 *` to avoid packed-struct alignment warnings
  - `udp_checksum`: builds pseudo-header as a plain `u8[12]` array (src_ip, dst_ip, zero, proto=17, udp_len in network byte order), runs `csum_add` over pseudo-header + UDP header + data; returns 0xFFFF if result would be 0
  - `udp_rx`: registered as IPv4 UDP handler; validates length, dispatches to first exact port match, then catch-all (port 0), logs a DEBUG drop if no handler found
  - `udp_register`: appends to handler table, returns -1 if full
  - `udp_send`: kmallocs header+data, fills fields in network byte order, computes checksum, calls `ipv4_send(dst_ip, IPPROTO_UDP, ...)`
  - `udp_init_impl`: calls `ipv4_register(IPPROTO_UDP, udp_rx)`
- `src/core/module_registry.c` — added `udp.h` include and `&mod_udp` after `&mod_icmp`

## Key decisions

- **Byte-by-byte checksum instead of u16 * cast** — casting a packed struct pointer to `u16 *` triggers `-Waddress-of-packed-member`; the `csum_add` helper sums big-endian byte pairs via `u8 *` pointer arithmetic, which is alignment-safe and produces identical results
- **Pseudo-header as plain u8[12]** — avoids the packed struct pointer cast entirely; src/dst IP written byte-by-byte in network byte order
- **Port 0 as catch-all** — a handler registered on port 0 receives all datagrams that have no exact port match; useful for future DHCP client (receives on port 68 but can also catch all if needed)
- **checksum=0xFFFF on all-zeros result** — per RFC 768, a computed checksum of 0 must be transmitted as 0xFFFF; 0 in the checksum field means "not computed"

## Verified build output

```
[INFO] [icmp] ready
[INFO] [udp] ready
[INFO] [kernel] All modules initialized.
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

PROMPT_27 — Phase 7 Step 7: `src/net/tcp/` — full TCP state machine, sliding window, retransmit.

---

[← 25](PROMPT_25.md) | [index](README.md) | **26** | [27 →](PROMPT_27.md)

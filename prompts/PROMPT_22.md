[← 21](PROMPT_21.md) | [index](README.md) | **22** | [23 →](PROMPT_23.md)

---

# PROMPT_22 — Phase 7 Step 2: Ethernet layer

**Session date:** 2026-03-29
**Status when starting:** Phase 7 Step 1 complete (e1000 driver ready, MAC 52:54:00:12:34:56)
**Status when done:** Phase 7 Step 2 complete — `src/net/ethernet/` builds clean; ethertype dispatch wired to e1000 RX callback; zero warnings

## What was built

- `src/net/ethernet/ethernet_internal.h` — EtherType constants (IPv4/ARP/IPv6), frame size limits, packed `eth_hdr_t` (dst[6] + src[6] + type u16), inline `htons`/`ntohs`/`htonl`/`ntohl` byte-order helpers, `ETH_MAX_HANDLERS` = 8
- `src/net/ethernet/ethernet.h` — public API: `ETHERTYPE_*` constants, `ETH_BROADCAST` MAC, `eth_rx_handler_t` callback type (`payload`, `len`, `src_mac`), `ethernet_init`, `ethernet_register(ethertype, handler)`, `ethernet_send(dst, type, payload, len)`, `ethernet_poll()`, `ethernet_get_mac(mac[6])`, `mod_ethernet`
- `src/net/ethernet/ethernet.c` — full implementation:
  - Static `eth_entry_t handlers[8]` dispatch table
  - `ethernet_rx(buf, len)` — e1000 RX callback: parse `eth_hdr_t`, `ntohs` the type, dispatch to registered handler, `kfree(buf)`
  - `ethernet_register` — append ethertype+handler to table, return -1 if full
  - `ethernet_send` — `kmalloc` a frame buffer, fill `eth_hdr_t` (dst, our_mac, `htons(type)`), copy payload, `e1000_send`, `kfree`
  - `ethernet_poll` — thin wrapper over `e1000_poll()`
  - `ethernet_init_impl` — call `e1000_get_mac`, register `ethernet_rx` with `e1000_register_rx_callback`
- `src/core/module_registry.c` — added `ethernet.h` include and `&mod_ethernet` after `&mod_e1000`

## Key decisions

- **Inline byte-order helpers in the internal header** — `htons`/`ntohs`/`htonl`/`ntohl` are needed by all net layers (ARP, IPv4, TCP); placing them in `ethernet_internal.h` once avoids duplication and keeps them close to the struct they serve
- **e1000 `kfree` responsibility lies in ethernet_rx** — the e1000 driver allocates a heap copy per frame and passes ownership to the callback; the ethernet layer is that callback and always calls `kfree(buf)` before returning, regardless of whether a handler was found
- **`ethernet_poll` wraps `e1000_poll`** — upper layers call `ethernet_poll()`; they never touch the NIC directly; this keeps the coupling one-directional
- **No promiscuous/multicast filtering at this layer** — ARP and IPv4 handlers decide relevance; the ethernet layer dispatches all frames to registered handlers unconditionally

## Verified build output

```
[INFO] [e1000] ready — MAC 52:54:0:12:34:56
[INFO] [ethernet] ready — MAC 52:54:0:12:34:56
[INFO] [kernel] All modules initialized.
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

[PROMPT_23 →](PROMPT_23.md) — Phase 7 Step 3: `src/net/arp/` — ARP request/reply, ARP table.

---

[← 21](PROMPT_21.md) | [index](README.md) | **22** | [23 →](PROMPT_23.md)

[← 28](PROMPT_28.md) | [index](README.md) | **29** | [30 →](PROMPT_30.md)

---

# PROMPT_29 — Phase 7 Step 9: DHCP

**Session date:** 2026-03-29
**Status when starting:** Phase 7 Step 8 complete (BSD socket syscalls)
**Status when done:** Phase 7 Step 9 complete — DHCP client; kernel obtains IP `10.0.2.15` from QEMU SLIRP on boot; zero warnings

## What was built

- `src/net/dhcp/dhcp_internal.h` — constants: `DHCP_XID=0xdeadbeef`, `DHCP_CLIENT_PORT=68`, `DHCP_SERVER_PORT=67`, `DHCP_MAGIC_COOKIE=0x63825363`, `DHCP_PKT_LEN=576`, `DHCP_HDR_LEN=236`, `DHCP_MIN_SIZE=300`, `DHCP_RETRY_MS=2000`, `DHCP_MAX_RETRIES=5`, op codes, option codes; `dhcp_state_t` enum (IDLE/SELECTING/REQUESTING/BOUND/FAILED); packed `dhcp_hdr_t` (236 bytes)
- `src/net/dhcp/dhcp.h` — `dhcp_tick`, `dhcp_bound`, `mod_dhcp`
- `src/net/dhcp/dhcp.c` — full DHCP client:
  - `opt_u8` / `opt_u32`: option-writing helpers
  - `send_discover`: builds BOOTREQUEST with DHCP Discover + parameter request list (subnet/router/DNS); sends to broadcast via `udp_send`
  - `send_request`: builds BOOTREQUEST with DHCP Request + requested-IP + server-ID options
  - `parse_options`: iterates TLV option chain; extracts MSGTYPE, SERVERID, SUBNET, ROUTER
  - `dhcp_rx` (UDP port 68 callback): validates op/xid/magic; handles OFFER → sends REQUEST; handles ACK → calls `ipv4_set_addr`; handles NAK → restarts
  - `dhcp_tick`: called from `net_poll` every timer tick; drives retransmit on timeout; gives up after `DHCP_MAX_RETRIES`
  - `dhcp_bound`: returns 1 when state == BOUND
  - `net_poll`: registered as timer callback; calls `ethernet_poll` + `dhcp_tick` + `tcp_tick` each tick
  - `dhcp_init_impl`: registers UDP handler on port 68, registers `net_poll` as timer callback, kicks off initial DISCOVER
- `src/core/module_registry.c` — added `dhcp.h` include and `&mod_dhcp` after `&mod_socket`

## Key decisions

- **`net_poll` lives in dhcp.c** — it was already there from the socket session (driving TCP tick); DHCP tick was added alongside it; no new file needed
- **`flags = 0x0000` (unicast flag)** — QEMU SLIRP ignores the broadcast flag and responds to unicast DHCP correctly; setting broadcast flag made no difference
- **Gateway fallback** — if DHCP ACK contains no router option, fall back to using the server IP as the default gateway; SLIRP sends router option but the fallback is robust

## Debugging story

The DHCP module was written and compiled cleanly but received no OFFER. The investigation proceeded in layers:

1. **Physical addresses confirmed correct** — `vmm_get_phys` handles the boot 1 GB identity pages; heap allocations at low physical addresses pass through unchanged; `rx_descs_phys=0x152400` and `rx_bufs_phys[0]=0x153800` were valid
2. **SLIRP confirmed non-responsive via pcap** — `filter-dump` showed only outgoing DISCOVER packets; no OFFER came back
3. **ARP probe confirmed RX hardware works** — `arp_request(10.0.2.2)` → SLIRP replied → frame arrived in e1000 RX ring → dispatched up the stack; proved DMA and polling were correct
4. **Zero-checksum bypass confirmed the root cause** — `hdr->checksum = 0` (which causes SLIRP to skip UDP checksum validation) immediately produced a complete DISCOVER→OFFER→REQUEST→ACK handshake
5. **Root cause: byte-order mismatch in `csum_add`** — the old checksum helper used explicit big-endian byte access (`buf[0]<<8 | buf[1]`), but SLIRP's `cksum.c` uses native little-endian `u16` reads (`*w++`); on x86 these produce different one's-complement sums, so SLIRP rejected every UDP packet from our kernel
6. **Fix: replaced `csum_add` with `csum_native`** — uses `__builtin_memcpy` into a `u16` for native byte-order reads, matching SLIRP's algorithm; the pseudo-header bytes are still laid out in network order but are now summed the same way both sides verify them

## Verified build output

```
[INFO] [udp] ready
[INFO] [tcp] ready (8 slots)
[INFO] [socket] ready (16 slots, fd base 64)
[INFO] [dhcp] DISCOVER sent (xid=0xdeadbeef)
[INFO] [dhcp] client started
[INFO] [dhcp] OFFER 10.0.2.15 from server 10.0.2.2
[INFO] [dhcp] REQUEST sent for 10.0.2.15
[INFO] [ipv4] address set: 10.0.2.15/255.255.255.0 gw 10.0.2.2
[INFO] [dhcp] bound — IP 10.0.2.15 mask 255.255.255.0 gw 10.0.2.2
```
(zero warnings, zero errors; all previous module lines unchanged)

## Phase 7 complete ✓

1. ✅ e1000 NIC driver (PCI detect, DMA TX/RX rings, polled mode)
2. ✅ Ethernet layer (frame parser/builder, ethertype dispatch)
3. ✅ ARP (request/reply, 16-entry table)
4. ✅ IPv4 (header, checksum, routing, broadcast)
5. ✅ ICMP (echo request/reply)
6. ✅ UDP (datagrams, port dispatch, native-byte-order checksum)
7. ✅ TCP (state machine, sliding window, retransmit)
8. ✅ Socket (BSD socket syscalls over TCP/UDP)
9. ✅ DHCP client (obtains IP 10.0.2.15/24 gw 10.0.2.2 from QEMU SLIRP on boot)

## Next session

[PROMPT_30 →](PROMPT_30.md) — Phase 8 Step 1: `src/smp/` — APIC SIPI, per-CPU data (gs-based), spinlocks

---

[← 28](PROMPT_28.md) | [index](README.md) | **29** | [30 →](PROMPT_30.md)

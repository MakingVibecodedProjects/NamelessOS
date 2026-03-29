[← 22](PROMPT_22.md) | [index](README.md) | **23** | [24 →](PROMPT_24.md)

---

# PROMPT_23 — Phase 7 Step 3: ARP

**Session date:** 2026-03-29
**Status when starting:** Phase 7 Step 2 complete (Ethernet layer, ethertype dispatch)
**Status when done:** Phase 7 Step 3 complete — ARP request/reply, 16-entry table, registered as Ethernet ARP handler; zero warnings

## What was built

- `src/net/arp/arp_internal.h` — hardware/protocol type constants (`HTYPE_ETHERNET=1`, `PTYPE_IPV4=0x0800`), opcode constants (`OP_REQUEST=1`, `OP_REPLY=2`), packed `arp_pkt_t` (28-byte Ethernet/IPv4 ARP packet), `arp_entry_t` (ip + mac[6]), `ARP_TABLE_SIZE=16`
- `src/net/arp/arp.h` — public API: `arp_init`, `arp_lookup(ip, mac[6])`, `arp_insert(ip, mac[6])`, `arp_request(ip)`, `mod_arp`
- `src/net/arp/arp.c` — full implementation:
  - `table[16]` static ARP cache, `our_ip` (set to 0 until DHCP assigns one)
  - `arp_lookup`: linear scan, return 0+copy MAC on hit, -1 on miss
  - `arp_insert`: update existing entry or find empty slot; slot-0 eviction when full
  - `arp_request`: build ARP request with `sha`=our_mac, `spa`=our_ip, `tha`=zero, `tpa`=target IP; send to Ethernet broadcast
  - `arp_rx` (static, registered with Ethernet): validate header fields; always learn sender's MAC→IP; if opcode=REQUEST and target=our_ip → build and send ARP reply
  - `arp_init_impl`: zero table, call `ethernet_register(ETHERTYPE_ARP, arp_rx)`
- `src/core/module_registry.c` — added `arp.h` include and `&mod_arp` after `&mod_ethernet`

## Key decisions

- **`our_ip = 0` until DHCP** — ARP reply logic is gated on `our_ip != 0`; no reply is sent before we have an IP; `our_ip` will be set by the DHCP/IPv4 layer in a later phase
- **Learn-on-receive** — every ARP packet (request or reply) updates the table for the sender's IP, not just replies; this is standard behaviour and reduces ARP traffic
- **Slot-0 eviction** — table-full case overwrites slot 0; sufficient for Phase 7 where the table will rarely exceed 2–3 entries
- **`ntohl`/`htonl` from ethernet_internal.h** — byte-order helpers live in the ethernet layer's internal header; ARP includes `ethernet_internal.h` to reuse them without redeclaration

## Verified build output

```
[INFO] [ethernet] ready — MAC 52:54:0:12:34:56
[INFO] [arp] ready (16-entry table)
[INFO] [kernel] All modules initialized.
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

[PROMPT_24 →](PROMPT_24.md) — Phase 7 Step 4: `src/net/ipv4/` — IPv4 header parser, routing table, IP address assignment.

---

[← 22](PROMPT_22.md) | [index](README.md) | **23** | [24 →](PROMPT_24.md)

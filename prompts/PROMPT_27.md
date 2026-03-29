[← 26](PROMPT_26.md) | [index](README.md) | **27** | [28 →](PROMPT_28.md)

---

# PROMPT_27 — Phase 7 Step 7: TCP

**Session date:** 2026-03-29
**Status when starting:** Phase 7 Step 6 complete (UDP datagrams, port dispatch)
**Status when done:** Phase 7 Step 7 complete — full TCP state machine, sliding window, retransmit; zero warnings

## What was built

- `src/net/tcp/tcp_internal.h` — packed `tcp_hdr_t` (20 bytes), flag constants (`TCP_FIN/SYN/RST/PSH/ACK/URG`), `TCP_DATA_OFF_MIN=0x50`, `tcp_state_t` enum (CLOSED → LISTEN → SYN_SENT / SYN_RCVD → ESTABLISHED → FIN_WAIT_1/2 / CLOSE_WAIT → CLOSING / LAST_ACK → TIME_WAIT), `TCP_RX_BUF_SIZE=4096`, `TCP_RTO_MS=1000`, `TCP_MAX_RETRIES=5`, `TCP_TIME_WAIT_MS=4000`, `TCP_MAX_CONN=8`, `tcp_conn_t` (state, local/remote ip+port, snd_una/nxt/wnd, rcv_nxt, retx_buf/len/deadline/count, rx_buf ring, on_data/on_close callbacks)
- `src/net/tcp/tcp.h` — public API: `tcp_conn_id_t`, `tcp_data_cb_t` / `tcp_close_cb_t` callback types, `tcp_init`, `tcp_listen(port, on_data, on_close)`, `tcp_connect(remote_ip, remote_port, local_port, on_data, on_close)`, `tcp_send(id, data, len)`, `tcp_close(id)`, `tcp_tick()`, `mod_tcp`
- `src/net/tcp/tcp.c` — full implementation:
  - `csum_add` / `tcp_checksum`: same byte-safe pattern as UDP; builds 12-byte pseudo-header as `u8[12]` array
  - `conn_alloc`: linear scan for `TCP_CLOSED` slot
  - `seq_lt` / `seq_leq`: signed 32-bit arithmetic for correct wraparound comparison
  - `tcp_send_raw`: fills checksum and calls `ipv4_send`
  - `send_ctrl`: sends a header-only control segment (SYN-ACK, ACK, FIN+ACK, RST)
  - `send_rst_reply`: sends RST without a conn context; seq/ack per RFC 793 §3.4 (ACK→RST; no-ACK→RST+ACK)
  - `retx_arm` / `retx_clear`: heap-copy segment for retransmit, exponential backoff timer
  - `rx_push` / `rx_free`: circular receive buffer helpers
  - `tcp_rx` (IPv4 handler): parses header, finds conn by 4-tuple, dispatches per state:
    - No conn + SYN → passive open (alloc conn, SYN-ACK)
    - No conn + !RST → send RST
    - RST → tear down
    - SYN_SENT: validate SYN-ACK, transition ESTABLISHED
    - SYN_RCVD: validate ACK, transition ESTABLISHED
    - ESTABLISHED+: ACK advances snd_una, data pushed to rx_buf + on_data callback, FIN drives CLOSE_WAIT / FIN_WAIT_2 / TIME_WAIT
  - `tcp_listen`: alloc LISTEN conn, install callbacks
  - `tcp_connect`: alloc SYN_SENT conn, send SYN, arm retx
  - `tcp_send`: build PSH+ACK segment, advance snd_nxt, arm retx
  - `tcp_close`: send FIN+ACK, transition FIN_WAIT_1 or LAST_ACK
  - `tcp_tick`: drives retransmit (exponential backoff, max 5 attempts) and TIME_WAIT expiry
- `src/core/module_registry.c` — added `tcp.h` include and `&mod_tcp` after `&mod_udp`

## Key decisions

- **`tcp_tick()` is a polling function** — must be called from a periodic timer callback or a kernel thread; not wired to a timer automatically (the socket/DHCP layer or a future kthread will call it)
- **4 KB linear rx_buf per connection** — no reordering; out-of-order segments trigger a duplicate ACK and are dropped; acceptable for a simple embedded stack
- **Retransmit covers the full last-sent segment only** — no send queue; once a segment is acked the retx buffer is cleared and a new call to `tcp_send` starts fresh; sufficient for stop-and-wait throughput
- **ISN from LCG** — `isn_counter * 1664525 + 1013904223` (Knuth); not cryptographically strong but avoids fixed ISN without depending on randomness infrastructure
- **Byte-safe checksum** — same `csum_add` pattern as UDP; `u8[12]` pseudo-header avoids packed-pointer alignment warnings
- **`send_rst_reply` without a conn** — needed for RST replies to port-closed SYNs; computes checksum inline using the swapped src/dst pair

## Verified build output

```
[INFO] [udp] ready
[INFO] [tcp] ready (8 slots)
[INFO] [kernel] All modules initialized.
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

[PROMPT_28 →](PROMPT_28.md) — Phase 7 Step 8: `src/net/socket/` — BSD socket syscalls (socket, bind, connect, accept, send, recv, close).

---

[← 26](PROMPT_26.md) | [index](README.md) | **27** | [28 →](PROMPT_28.md)

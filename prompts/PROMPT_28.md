[← 27](PROMPT_27.md) | [index](README.md) | **28** | [29 →](PROMPT_29.md)

---

# PROMPT_28 — Phase 7 Step 8: Socket

**Session date:** 2026-03-29
**Status when starting:** Phase 7 Step 7 complete (TCP state machine, sliding window, retransmit)
**Status when done:** Phase 7 Step 8 complete — BSD socket syscalls (socket/bind/listen/accept/connect/sendto/recvfrom/shutdown); zero warnings

## What was built

- `src/net/socket/socket_internal.h` — `AF_INET=2`, `SOCK_STREAM=1`, `SOCK_DGRAM=2`, Linux-ABI syscall numbers (41–50), error codes (EINVAL, ENOMEM, EAFNOSUPPORT, EPROTONOSUPPORT, ENOTSOCK, ENOTCONN, EISCONN, EADDRINUSE, EOPNOTSUPP, EAGAIN), packed `sockaddr_in_t` (16 bytes: sin_family, sin_port, sin_addr, sin_zero[8]), `sock_type_t` enum, `sock_t` (type, conn_id, bound_port, peer ip/port, 2 KB UDP rx ring), `SOCK_MAX=16`, `SOCK_FD_BASE=64`
- `src/net/socket/socket.h` — `socket_init`, `mod_socket`
- `src/net/socket/socket.c` — full implementation:
  - `udp_sock_rx`: UDP callback that routes datagrams into the bound socket's rx ring and saves peer address
  - `tcp_sock_data` / `tcp_sock_close`: TCP callbacks (data already in tcp_conn_t.rx_buf; close clears conn_id)
  - `sys_socket`: allocs `sock_t`, sets type, returns `SOCK_FD_BASE + idx` as fd
  - `sys_bind`: validates `sockaddr_in`, checks port reuse, stores bound_port; for UDP calls `udp_register` immediately
  - `sys_listen`: calls `tcp_listen`, stores conn_id in LISTEN socket
  - `sys_accept`: calls `tcp_accept` (polls for ESTABLISHED conn on port), allocs new socket fd, optionally fills `sockaddr_in` with peer address
  - `sys_connect`: calls `tcp_connect` with auto-assigned ephemeral port if not bound
  - `sys_sendto`: TCP → `tcp_send`; UDP → `udp_send` with explicit addr or saved peer
  - `sys_recvfrom`: TCP → `tcp_read`; UDP → drain from rx ring, fill peer `sockaddr_in`; both return `-EAGAIN` if no data
  - `sys_shutdown`: calls `tcp_close`, marks socket SOCK_FREE
  - `socket_init_impl`: calls `syscall_register` for each syscall number (41–50)
- `src/net/tcp/tcp.h` — added `tcp_accept(listen_port)`, `tcp_read(id, buf, len)`, `tcp_get_peer(id, ip, port)`
- `src/net/tcp/tcp.c` — implemented the three new functions:
  - `tcp_accept`: scans for ESTABLISHED conn on listen_port with non-NULL `on_data`; clears `on_data` to mark as claimed
  - `tcp_read`: drains bytes from `tcp_conn_t.rx_buf` ring into caller buffer
  - `tcp_get_peer`: copies remote_ip / remote_port from conn
- `src/syscall/syscall.h` — added `syscall_register(nr, fn)` declaration
- `src/syscall/syscall.c` — added `syscall_register` implementation (one-liner: writes into `dispatch_table[nr]`)
- `src/core/module_registry.c` — added `socket.h` include and `&mod_socket` after `&mod_tcp`

## Key decisions

- **fd base at 64** — socket fds live in 64–79; keeps them clear of VFS fds (0–63); no integration with VFS fd table needed at this stage
- **Polled accept** — `tcp_accept` scans for ESTABLISHED with `on_data != NULL` as the "unclaimed" marker; simple and avoids a separate accept queue
- **UDP rx ring per socket** — 2 KB per socket; UDP data is consumed by `recvfrom` from this ring; the ring is filled by `udp_sock_rx` which is registered at `bind` time
- **`syscall_register` added to syscall module** — lets any module install handlers after `syscall_init` without touching `syscall.c`; needed so the socket module can self-register at init time
- **`-EAGAIN` for non-blocking semantics** — both `recvfrom` and `accept` return `-EAGAIN` (11) immediately if no data/connection is ready; userspace must poll or spin

## Verified build output

```
[INFO] [tcp] ready (8 slots)
[INFO] [socket] ready (16 slots, fd base 64)
[INFO] [kernel] All modules initialized.
```
(zero warnings, zero errors; all previous module lines unchanged)

## Next session

[PROMPT_29 →](PROMPT_29.md) — Phase 7 Step 9: `src/net/dhcp/` — DHCP client, obtain IP on boot.

---

[← 27](PROMPT_27.md) | [index](README.md) | **28** | [29 →](PROMPT_29.md)

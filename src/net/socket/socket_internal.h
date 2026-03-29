/* socket_internal.h — private constants and socket table for the socket layer */
#ifndef SOCKET_INTERNAL_H
#define SOCKET_INTERNAL_H

#include "../../lib/types.h"

/* ── Domain / type / protocol constants (Linux ABI) ─────────────────── */
#define AF_INET         2
#define SOCK_STREAM     1       /* TCP */
#define SOCK_DGRAM      2       /* UDP */

/* ── Linux-compatible syscall numbers ───────────────────────────────── */
#define SYS_SOCKET      41
#define SYS_CONNECT     42
#define SYS_ACCEPT      43
#define SYS_SENDTO      44
#define SYS_RECVFROM    45
#define SYS_SHUTDOWN    48
#define SYS_BIND        49
#define SYS_LISTEN      50

/* ── Error codes (supplement syscall_internal.h) ────────────────────── */
#define EINVAL          22
#define ENOMEM          12
#define EAFNOSUPPORT    97
#define EPROTONOSUPPORT 93
#define ENOTSOCK        88
#define ENOTCONN        107
#define EISCONN         106
#define EADDRINUSE      98
#define EOPNOTSUPP      95
#define EAGAIN          11

/* ── sockaddr_in layout (matches userspace struct) ───────────────────── */
typedef struct __attribute__((packed)) {
    u16 sin_family;     /* AF_INET = 2 */
    u16 sin_port;       /* network byte order */
    u32 sin_addr;       /* network byte order */
    u8  sin_zero[8];    /* padding */
} sockaddr_in_t;

/* ── Socket types ────────────────────────────────────────────────────── */
typedef enum {
    SOCK_FREE = 0,
    SOCK_TCP,
    SOCK_UDP,
} sock_type_t;

/* ── Per-socket state ────────────────────────────────────────────────── */
typedef struct {
    sock_type_t type;
    int         conn_id;    /* TCP: tcp_conn_id_t; UDP: bound port or -1 */
    u16         bound_port; /* host byte order; 0 = not bound */
    u32         peer_ip;    /* UDP: last sendto destination */
    u16         peer_port;  /* UDP: last sendto port (host byte order) */

    /* UDP receive ring */
    u8  rx_buf[2048];
    u16 rx_head;
    u16 rx_tail;
} sock_t;

/* ── Socket table ────────────────────────────────────────────────────── */
#define SOCK_MAX        16
#define SOCK_FD_BASE    64      /* fd 64..79 reserved for sockets */

#endif /* SOCKET_INTERNAL_H */

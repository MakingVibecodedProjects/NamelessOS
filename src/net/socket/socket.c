#include "socket.h"
#include "socket_internal.h"
#include "../tcp/tcp.h"
#include "../udp/udp.h"
#include "../ipv4/ipv4.h"
#include "../ethernet/ethernet_internal.h"
#include "../../serial/serial.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

/* ── Forward declaration of syscall installer ────────────────────────── */
extern void syscall_register(u32 nr, u64 (*fn)(u64,u64,u64,u64,u64,u64));

/* ── Socket table ────────────────────────────────────────────────────── */
static sock_t socks[SOCK_MAX];

/* ── Helpers ─────────────────────────────────────────────────────────── */
static int fd_to_idx(int fd) {
    int idx = fd - SOCK_FD_BASE;
    if (idx < 0 || idx >= SOCK_MAX) return -1;
    return idx;
}

static int alloc_sock(void) {
    for (int i = 0; i < SOCK_MAX; i++)
        if (socks[i].type == SOCK_FREE) return i;
    return -1;
}

/* ── UDP receive callback ─────────────────────────────────────────────── */
static void udp_sock_rx(const u8 *data, u16 len,
                        u32 src_ip, u16 src_port, u16 dst_port) {
    /* Find socket bound to dst_port */
    for (int i = 0; i < SOCK_MAX; i++) {
        sock_t *s = &socks[i];
        if (s->type != SOCK_UDP) continue;
        if (s->bound_port != dst_port) continue;

        /* Save peer for recvfrom */
        s->peer_ip   = src_ip;
        s->peer_port = src_port;

        /* Push into ring — drop if full */
        u16 free = (u16)(sizeof(s->rx_buf) - 1 -
                         (u16)((s->rx_tail - s->rx_head +
                                sizeof(s->rx_buf)) % sizeof(s->rx_buf)));
        if (len > free) len = free;
        for (u16 j = 0; j < len; j++) {
            s->rx_buf[s->rx_tail] = data[j];
            s->rx_tail = (u16)((s->rx_tail + 1) % sizeof(s->rx_buf));
        }
        return;
    }
}

/* ── TCP data/close callbacks ─────────────────────────────────────────── */
static void tcp_sock_data(tcp_conn_id_t id, const u8 *data, u16 len) {
    /* Find socket by conn_id */
    for (int i = 0; i < SOCK_MAX; i++) {
        sock_t *s = &socks[i];
        if (s->type == SOCK_TCP && s->conn_id == id) {
            (void)data; (void)len;
            /* Data is already buffered in tcp_conn_t.rx_buf — nothing extra to do */
            return;
        }
    }
}

static void tcp_sock_close(tcp_conn_id_t id) {
    for (int i = 0; i < SOCK_MAX; i++) {
        sock_t *s = &socks[i];
        if (s->type == SOCK_TCP && s->conn_id == (int)id) {
            s->conn_id = -1;
            return;
        }
    }
}

/* ── sys_socket(domain, type, protocol) → fd ─────────────────────────── */
static u64 sys_socket(u64 domain, u64 type, u64 proto __attribute__((unused)),
                      u64 a3 __attribute__((unused)),
                      u64 a4 __attribute__((unused)),
                      u64 a5 __attribute__((unused))) {
    if (domain != AF_INET) return (u64)(i64)(-EAFNOSUPPORT);
    if (type != SOCK_STREAM && type != SOCK_DGRAM)
        return (u64)(i64)(-EPROTONOSUPPORT);

    int idx = alloc_sock();
    if (idx < 0) return (u64)(i64)(-ENOMEM);

    sock_t *s     = &socks[idx];
    s->type       = (type == SOCK_STREAM) ? SOCK_TCP : SOCK_UDP;
    s->conn_id    = -1;
    s->bound_port = 0;
    s->peer_ip    = 0;
    s->peer_port  = 0;
    s->rx_head    = 0;
    s->rx_tail    = 0;

    return (u64)(SOCK_FD_BASE + idx);
}

/* ── sys_bind(fd, sockaddr_in *, addrlen) → 0 or error ──────────────── */
static u64 sys_bind(u64 fd, u64 addr, u64 addrlen __attribute__((unused)),
                    u64 a3 __attribute__((unused)),
                    u64 a4 __attribute__((unused)),
                    u64 a5 __attribute__((unused))) {
    int idx = fd_to_idx((int)fd);
    if (idx < 0) return (u64)(i64)(-ENOTSOCK);
    sock_t *s = &socks[idx];
    if (s->type == SOCK_FREE) return (u64)(i64)(-ENOTSOCK);

    const sockaddr_in_t *sa = (const sockaddr_in_t *)(usize)addr;
    if (sa->sin_family != AF_INET) return (u64)(i64)(-EAFNOSUPPORT);

    u16 port = ntohs(sa->sin_port);
    /* Check for port reuse */
    for (int i = 0; i < SOCK_MAX; i++) {
        if (i == idx) continue;
        if (socks[i].type != SOCK_FREE && socks[i].bound_port == port)
            return (u64)(i64)(-EADDRINUSE);
    }
    s->bound_port = port;

    /* UDP: register handler now */
    if (s->type == SOCK_UDP)
        udp_register(port, udp_sock_rx);

    return 0;
}

/* ── sys_listen(fd, backlog) → 0 or error ────────────────────────────── */
static u64 sys_listen(u64 fd, u64 backlog __attribute__((unused)),
                      u64 a2 __attribute__((unused)),
                      u64 a3 __attribute__((unused)),
                      u64 a4 __attribute__((unused)),
                      u64 a5 __attribute__((unused))) {
    int idx = fd_to_idx((int)fd);
    if (idx < 0) return (u64)(i64)(-ENOTSOCK);
    sock_t *s = &socks[idx];
    if (s->type != SOCK_TCP) return (u64)(i64)(-EOPNOTSUPP);
    if (s->bound_port == 0)  return (u64)(i64)(-EINVAL);

    tcp_conn_id_t id = tcp_listen(s->bound_port, tcp_sock_data, tcp_sock_close);
    if (id < 0) return (u64)(i64)(-EINVAL);
    s->conn_id = id;
    return 0;
}

/* ── sys_accept(fd, addr, addrlen) → new_fd ──────────────────────────── */
/* Polled accept: scans for an ESTABLISHED conn on our listen port */
static u64 sys_accept(u64 fd, u64 addr, u64 addrlen __attribute__((unused)),
                      u64 a3 __attribute__((unused)),
                      u64 a4 __attribute__((unused)),
                      u64 a5 __attribute__((unused))) {
    int idx = fd_to_idx((int)fd);
    if (idx < 0) return (u64)(i64)(-ENOTSOCK);
    sock_t *s = &socks[idx];
    if (s->type != SOCK_TCP) return (u64)(i64)(-EOPNOTSUPP);

    /* Find an ESTABLISHED tcp conn on our listen port that has no socket yet */
    tcp_conn_id_t found = tcp_accept(s->bound_port);
    if (found < 0) return (u64)(i64)(-EAGAIN);

    int nidx = alloc_sock();
    if (nidx < 0) return (u64)(i64)(-ENOMEM);

    sock_t *ns    = &socks[nidx];
    ns->type      = SOCK_TCP;
    ns->conn_id   = found;
    ns->bound_port = s->bound_port;
    ns->rx_head   = 0;
    ns->rx_tail   = 0;

    /* Fill in peer address if caller provided a buffer */
    if (addr) {
        sockaddr_in_t *sa = (sockaddr_in_t *)(usize)addr;
        u32 pip; u16 pport;
        tcp_get_peer(found, &pip, &pport);
        sa->sin_family = AF_INET;
        sa->sin_port   = htons(pport);
        sa->sin_addr   = htonl(pip);
    }

    return (u64)(SOCK_FD_BASE + nidx);
}

/* ── sys_connect(fd, sockaddr_in *, addrlen) → 0 or error ───────────── */
static u64 sys_connect(u64 fd, u64 addr,
                       u64 addrlen __attribute__((unused)),
                       u64 a3 __attribute__((unused)),
                       u64 a4 __attribute__((unused)),
                       u64 a5 __attribute__((unused))) {
    int idx = fd_to_idx((int)fd);
    if (idx < 0) return (u64)(i64)(-ENOTSOCK);
    sock_t *s = &socks[idx];
    if (s->type != SOCK_TCP) return (u64)(i64)(-EOPNOTSUPP);
    if (s->conn_id >= 0)     return (u64)(i64)(-EISCONN);

    const sockaddr_in_t *sa = (const sockaddr_in_t *)(usize)addr;
    u32 dst_ip   = ntohl(sa->sin_addr);
    u16 dst_port = ntohs(sa->sin_port);
    u16 src_port = s->bound_port ? s->bound_port : (u16)(49152 + (int)fd);

    tcp_conn_id_t id = tcp_connect(dst_ip, dst_port, src_port,
                                   tcp_sock_data, tcp_sock_close);
    if (id < 0) return (u64)(i64)(-EINVAL);
    s->conn_id = id;
    return 0;
}

/* ── sys_sendto(fd, buf, len, flags, addr, addrlen) → bytes or error ── */
static u64 sys_sendto(u64 fd, u64 buf, u64 len,
                      u64 flags __attribute__((unused)),
                      u64 addr, u64 addrlen __attribute__((unused))) {
    int idx = fd_to_idx((int)fd);
    if (idx < 0) return (u64)(i64)(-ENOTSOCK);
    sock_t *s = &socks[idx];

    if (s->type == SOCK_TCP) {
        if (s->conn_id < 0) return (u64)(i64)(-ENOTCONN);
        int ret = tcp_send(s->conn_id, (const u8 *)(usize)buf, (u16)len);
        return ret < 0 ? (u64)(i64)(-EINVAL) : (u64)len;
    }

    if (s->type == SOCK_UDP) {
        u32 dst_ip; u16 dst_port;
        if (addr) {
            const sockaddr_in_t *sa = (const sockaddr_in_t *)(usize)addr;
            dst_ip   = ntohl(sa->sin_addr);
            dst_port = ntohs(sa->sin_port);
        } else if (s->peer_ip) {
            dst_ip   = s->peer_ip;
            dst_port = s->peer_port;
        } else {
            return (u64)(i64)(-ENOTCONN);
        }
        int ret = udp_send(dst_ip, s->bound_port, dst_port,
                           (const u8 *)(usize)buf, (u16)len);
        return ret < 0 ? (u64)(i64)(-EINVAL) : (u64)len;
    }

    return (u64)(i64)(-ENOTSOCK);
}

/* ── sys_recvfrom(fd, buf, len, flags, addr, addrlen) → bytes or error ─ */
static u64 sys_recvfrom(u64 fd, u64 buf, u64 len,
                        u64 flags __attribute__((unused)),
                        u64 addr,
                        u64 addrlen __attribute__((unused))) {
    int idx = fd_to_idx((int)fd);
    if (idx < 0) return (u64)(i64)(-ENOTSOCK);
    sock_t *s = &socks[idx];

    if (s->type == SOCK_TCP) {
        if (s->conn_id < 0) return (u64)(i64)(-ENOTCONN);
        u16 got = tcp_read(s->conn_id, (u8 *)(usize)buf, (u16)len);
        if (got == 0) return (u64)(i64)(-EAGAIN);
        return (u64)got;
    }

    if (s->type == SOCK_UDP) {
        /* Drain from ring */
        u16 avail = (u16)((s->rx_tail - s->rx_head +
                           sizeof(s->rx_buf)) % sizeof(s->rx_buf));
        if (avail == 0) return (u64)(i64)(-EAGAIN);
        u16 n = (u16)(avail < (u16)len ? avail : (u16)len);
        u8 *out = (u8 *)(usize)buf;
        for (u16 i = 0; i < n; i++) {
            out[i]    = s->rx_buf[s->rx_head];
            s->rx_head = (u16)((s->rx_head + 1) % sizeof(s->rx_buf));
        }
        if (addr) {
            sockaddr_in_t *sa = (sockaddr_in_t *)(usize)addr;
            sa->sin_family = AF_INET;
            sa->sin_port   = htons(s->peer_port);
            sa->sin_addr   = htonl(s->peer_ip);
        }
        return (u64)n;
    }

    return (u64)(i64)(-ENOTSOCK);
}

/* ── sys_shutdown(fd, how) ────────────────────────────────────────────── */
static u64 sys_shutdown(u64 fd, u64 how __attribute__((unused)),
                        u64 a2 __attribute__((unused)),
                        u64 a3 __attribute__((unused)),
                        u64 a4 __attribute__((unused)),
                        u64 a5 __attribute__((unused))) {
    int idx = fd_to_idx((int)fd);
    if (idx < 0) return (u64)(i64)(-ENOTSOCK);
    sock_t *s = &socks[idx];
    if (s->type == SOCK_TCP && s->conn_id >= 0)
        tcp_close(s->conn_id);
    s->type    = SOCK_FREE;
    s->conn_id = -1;
    return 0;
}

/* ── Module init / dump ──────────────────────────────────────────────── */
static int socket_init_impl(void) {
    for (int i = 0; i < SOCK_MAX; i++) {
        socks[i].type    = SOCK_FREE;
        socks[i].conn_id = -1;
    }

    syscall_register(SYS_SOCKET,   sys_socket);
    syscall_register(SYS_BIND,     sys_bind);
    syscall_register(SYS_LISTEN,   sys_listen);
    syscall_register(SYS_ACCEPT,   sys_accept);
    syscall_register(SYS_CONNECT,  sys_connect);
    syscall_register(SYS_SENDTO,   sys_sendto);
    syscall_register(SYS_RECVFROM, sys_recvfrom);
    syscall_register(SYS_SHUTDOWN, sys_shutdown);

    klog(LOG_INFO, "[socket] ready (%d slots, fd base %d)\n",
         SOCK_MAX, SOCK_FD_BASE);
    return 0;
}

static void socket_dump(void) {
    int used = 0;
    for (int i = 0; i < SOCK_MAX; i++)
        if (socks[i].type != SOCK_FREE) used++;
    klog(LOG_INFO, "[socket] %d/%d sockets in use\n", used, SOCK_MAX);
}

/* ── Module descriptor ───────────────────────────────────────────────── */
kernel_module_t mod_socket = {
    .name        = "socket",
    .init        = socket_init_impl,
    .dump        = socket_dump,
    .initialized = false,
};

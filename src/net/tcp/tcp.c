#include "tcp.h"
#include "tcp_internal.h"
#include "../ipv4/ipv4.h"
#include "../ipv4/ipv4_internal.h"
#include "../ethernet/ethernet_internal.h"
#include "../../heap/heap.h"
#include "../../timer/timer.h"
#include "../../serial/serial.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

/* ── Connection table ────────────────────────────────────────────────── */
static tcp_conn_t conns[TCP_MAX_CONN];

/* ── Simple pseudo-random ISN (seeded from tick counter) ─────────────── */
static u32 isn_counter = 0x12345678u;
static u32 next_isn(void) {
    isn_counter = isn_counter * 1664525u + 1013904223u;
    return isn_counter;
}

/* ── csum_add (same pattern as UDP) ─────────────────────────────────── */
static u32 csum_add(u32 sum, const u8 *buf, u16 len) {
    while (len > 1) {
        sum += (u32)((u16)buf[0] << 8 | (u16)buf[1]);
        buf += 2;
        len  = (u16)(len - 2);
    }
    if (len)
        sum += (u32)((u16)buf[0] << 8);
    return sum;
}

/* ── tcp_checksum ────────────────────────────────────────────────────── */
static u16 tcp_checksum(u32 src_ip, u32 dst_ip,
                        const u8 *seg, u16 seg_len) {
    /* Pseudo-header */
    u8 ph[12];
    ph[0]  = (u8)(src_ip >> 24); ph[1]  = (u8)(src_ip >> 16);
    ph[2]  = (u8)(src_ip >>  8); ph[3]  = (u8)(src_ip);
    ph[4]  = (u8)(dst_ip >> 24); ph[5]  = (u8)(dst_ip >> 16);
    ph[6]  = (u8)(dst_ip >>  8); ph[7]  = (u8)(dst_ip);
    ph[8]  = 0;
    ph[9]  = IPPROTO_TCP;
    ph[10] = (u8)(seg_len >> 8); ph[11] = (u8)(seg_len);

    u32 sum = 0;
    sum = csum_add(sum, ph, 12);
    sum = csum_add(sum, seg, seg_len);
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    u16 result = (u16)~sum;
    return result ? result : 0xFFFF;
}

/* ── Helpers ─────────────────────────────────────────────────────────── */
static int conn_alloc(void) {
    for (int i = 0; i < TCP_MAX_CONN; i++)
        if (conns[i].state == TCP_CLOSED) return i;
    return -1;
}

/* SEQ arithmetic — handles wrap */
static int seq_lt(u32 a, u32 b)  { return (i32)(a - b) <  0; }
static int seq_leq(u32 a, u32 b) { return (i32)(a - b) <= 0; }

/* ── Send a raw TCP segment ──────────────────────────────────────────── */
/* seg_len = TCP header (20) + data length */
static int tcp_send_raw(tcp_conn_t *c, u8 *seg, u16 seg_len) {
    tcp_hdr_t *h = (tcp_hdr_t *)seg;
    h->checksum = 0;
    h->checksum = tcp_checksum(c->local_ip, c->remote_ip, seg, seg_len);
    return ipv4_send(c->remote_ip, IPPROTO_TCP, seg, seg_len);
}

/* ── Build and send a control segment (no data) ─────────────────────── */
static void send_ctrl(tcp_conn_t *c, u8 flags, u32 seq, u32 ack) {
    u8 seg[TCP_HDR_LEN];
    tcp_hdr_t *h = (tcp_hdr_t *)seg;
    h->src_port = htons(c->local_port);
    h->dst_port = htons(c->remote_port);
    h->seq      = htonl(seq);
    h->ack      = htonl(ack);
    h->data_off = TCP_DATA_OFF_MIN;
    h->flags    = flags;
    h->window   = htons(TCP_WINDOW);
    h->checksum = 0;
    h->urg_ptr  = 0;
    tcp_send_raw(c, seg, TCP_HDR_LEN);
}

/* ── Send RST in response to an unexpected segment ───────────────────── */
static void send_rst_reply(u32 src_ip, u16 src_port,
                           u32 dst_ip, u16 dst_port,
                           u32 seq, u32 ack, u8 flags) {
    u8 seg[TCP_HDR_LEN];
    tcp_hdr_t *h = (tcp_hdr_t *)seg;
    h->src_port = htons(dst_port);
    h->dst_port = htons(src_port);
    h->data_off = TCP_DATA_OFF_MIN;
    h->window   = 0;
    h->urg_ptr  = 0;
    if (flags & TCP_ACK) {
        h->seq   = htonl(ack);
        h->ack   = 0;
        h->flags = TCP_RST;
    } else {
        h->seq   = 0;
        h->ack   = htonl(seq + 1);
        h->flags = TCP_RST | TCP_ACK;
    }
    h->checksum = 0;

    /* Compute checksum inline — no conn available */
    u8 ph[12];
    ph[0]  = (u8)(dst_ip >> 24); ph[1]  = (u8)(dst_ip >> 16);
    ph[2]  = (u8)(dst_ip >>  8); ph[3]  = (u8)(dst_ip);
    ph[4]  = (u8)(src_ip >> 24); ph[5]  = (u8)(src_ip >> 16);
    ph[6]  = (u8)(src_ip >>  8); ph[7]  = (u8)(src_ip);
    ph[8]  = 0; ph[9] = IPPROTO_TCP;
    ph[10] = 0; ph[11] = TCP_HDR_LEN;
    u32 sum = 0;
    sum = csum_add(sum, ph, 12);
    sum = csum_add(sum, seg, TCP_HDR_LEN);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    h->checksum = (u16)~sum ? (u16)~sum : 0xFFFF;

    ipv4_send(src_ip, IPPROTO_TCP, seg, TCP_HDR_LEN);
}

/* ── Queue a segment for retransmit ─────────────────────────────────── */
static void retx_arm(tcp_conn_t *c, const u8 *seg, u16 len) {
    if (c->retx_buf) { kfree(c->retx_buf); c->retx_buf = (void *)0; }
    c->retx_buf      = kmalloc(len);
    if (!c->retx_buf) return;
    memcpy(c->retx_buf, seg, len);
    c->retx_len      = len;
    c->retx_deadline = timer_get_ticks() + TCP_RTO_MS;
    c->retx_count    = 0;
}

static void retx_clear(tcp_conn_t *c) {
    if (c->retx_buf) { kfree(c->retx_buf); c->retx_buf = (void *)0; }
    c->retx_len   = 0;
    c->retx_count = 0;
}

/* ── rx_buf helpers ──────────────────────────────────────────────────── */
static u16 rx_free(const tcp_conn_t *c) {
    return (u16)(TCP_RX_BUF_SIZE - 1 -
                 (u16)((c->rx_tail - c->rx_head + TCP_RX_BUF_SIZE)
                        % TCP_RX_BUF_SIZE));
}

static void rx_push(tcp_conn_t *c, const u8 *data, u16 len) {
    for (u16 i = 0; i < len; i++) {
        c->rx_buf[c->rx_tail] = data[i];
        c->rx_tail = (u16)((c->rx_tail + 1) % TCP_RX_BUF_SIZE);
    }
}

/* ── tcp_rx — registered as IPv4 TCP handler ────────────────────────── */
static void tcp_rx(const u8 *payload, u16 len,
                   u32 src_ip, u32 dst_ip) {
    if (len < TCP_HDR_LEN) return;

    const tcp_hdr_t *h      = (const tcp_hdr_t *)payload;
    u8               hdr_words = (u8)(h->data_off >> 4);
    u16              hdr_len   = (u16)(hdr_words * 4u);
    if (hdr_len < TCP_HDR_LEN || hdr_len > len) return;

    u16  src_port = ntohs(h->src_port);
    u16  dst_port = ntohs(h->dst_port);
    u32  seg_seq  = ntohl(h->seq);
    u32  seg_ack  = ntohl(h->ack);
    u16  seg_wnd  = ntohs(h->window);
    u8   flags    = h->flags;
    u16  data_len = (u16)(len - hdr_len);
    const u8 *data = payload + hdr_len;

    /* Find matching established/connecting connection first */
    tcp_conn_t *c    = (void *)0;
    tcp_conn_t *lstn = (void *)0;

    for (int i = 0; i < TCP_MAX_CONN; i++) {
        tcp_conn_t *t = &conns[i];
        if (t->state == TCP_CLOSED) continue;
        if (t->state == TCP_LISTEN && t->local_port == dst_port) {
            lstn = t;
            continue;
        }
        if (t->local_port  == dst_port &&
            t->remote_port == src_port &&
            t->remote_ip   == src_ip) {
            c = t;
            break;
        }
    }

    /* ── No matching connection ── */
    if (!c) {
        if (lstn && (flags & TCP_SYN) && !(flags & TCP_ACK)) {
            /* Passive open: allocate a new conn for this SYN */
            int id = conn_alloc();
            if (id < 0) {
                send_rst_reply(src_ip, src_port, dst_ip, dst_port,
                               seg_seq, seg_ack, flags);
                return;
            }
            tcp_conn_t *nc = &conns[id];
            nc->state       = TCP_SYN_RCVD;
            nc->local_ip    = dst_ip;
            nc->local_port  = dst_port;
            nc->remote_ip   = src_ip;
            nc->remote_port = src_port;
            nc->snd_una     = next_isn();
            nc->snd_nxt     = nc->snd_una + 1;
            nc->snd_wnd     = seg_wnd;
            nc->rcv_nxt     = seg_seq + 1;
            nc->rx_head     = 0;
            nc->rx_tail     = 0;
            nc->retx_buf    = (void *)0;
            nc->retx_len    = 0;
            nc->on_data     = lstn->on_data;
            nc->on_close    = lstn->on_close;

            /* Send SYN-ACK */
            u8 synack[TCP_HDR_LEN];
            tcp_hdr_t *sh = (tcp_hdr_t *)synack;
            sh->src_port = htons(nc->local_port);
            sh->dst_port = htons(nc->remote_port);
            sh->seq      = htonl(nc->snd_una);
            sh->ack      = htonl(nc->rcv_nxt);
            sh->data_off = TCP_DATA_OFF_MIN;
            sh->flags    = TCP_SYN | TCP_ACK;
            sh->window   = htons(TCP_WINDOW);
            sh->checksum = 0;
            sh->urg_ptr  = 0;
            retx_arm(nc, synack, TCP_HDR_LEN);
            tcp_send_raw(nc, synack, TCP_HDR_LEN);
            klog(LOG_DEBUG, "[tcp] SYN_RCVD :%d <- %d.%d.%d.%d:%d\n",
                 (int)dst_port,
                 (int)(src_ip>>24),(int)((src_ip>>16)&0xFF),
                 (int)((src_ip>>8)&0xFF),(int)(src_ip&0xFF),
                 (int)src_port);
        } else if (!(flags & TCP_RST)) {
            send_rst_reply(src_ip, src_port, dst_ip, dst_port,
                           seg_seq, seg_ack, flags);
        }
        return;
    }

    /* ── RST handling ── */
    if (flags & TCP_RST) {
        if (c->state == TCP_SYN_SENT) {
            /* RST in response to our SYN */
        } else if (!seq_lt(seg_seq, c->rcv_nxt) &&
                   !seq_leq(c->rcv_nxt + TCP_WINDOW, seg_seq)) {
            /* valid RST */
        } else {
            return; /* out of window — ignore */
        }
        retx_clear(c);
        if (c->on_close) c->on_close((int)(c - conns));
        c->state = TCP_CLOSED;
        return;
    }

    /* ── SYN_SENT: waiting for SYN-ACK ── */
    if (c->state == TCP_SYN_SENT) {
        if (!(flags & TCP_ACK)) return;
        if (seg_ack != c->snd_nxt) {
            send_rst_reply(src_ip, src_port, dst_ip, dst_port,
                           seg_seq, seg_ack, flags);
            return;
        }
        if (!(flags & TCP_SYN)) return;
        c->snd_una  = seg_ack;
        c->snd_wnd  = seg_wnd;
        c->rcv_nxt  = seg_seq + 1;
        retx_clear(c);
        c->state    = TCP_ESTABLISHED;
        send_ctrl(c, TCP_ACK, c->snd_nxt, c->rcv_nxt);
        klog(LOG_INFO, "[tcp] ESTABLISHED (active) :%d\n", (int)c->local_port);
        return;
    }

    /* ── SYN_RCVD: waiting for ACK of our SYN-ACK ── */
    if (c->state == TCP_SYN_RCVD) {
        if (!(flags & TCP_ACK)) return;
        if (seg_ack == c->snd_nxt) {
            c->snd_una = seg_ack;
            c->snd_wnd = seg_wnd;
            retx_clear(c);
            c->state   = TCP_ESTABLISHED;
            klog(LOG_INFO, "[tcp] ESTABLISHED (passive) :%d\n", (int)c->local_port);
        }
        /* Fall through: may also carry data */
        if (c->state != TCP_ESTABLISHED) return;
    }

    /* ── ESTABLISHED and beyond: process ACK + data + FIN ── */

    /* ACK processing */
    if (flags & TCP_ACK) {
        if (c->state == TCP_ESTABLISHED ||
            c->state == TCP_FIN_WAIT_1  ||
            c->state == TCP_CLOSE_WAIT  ||
            c->state == TCP_CLOSING) {
            if (seq_leq(c->snd_una, seg_ack) &&
                seq_leq(seg_ack, c->snd_nxt)) {
                c->snd_una = seg_ack;
                c->snd_wnd = seg_wnd;
                /* If our retx segment is now fully acked, clear it */
                if (c->retx_buf && seq_leq(c->snd_nxt, c->snd_una))
                    retx_clear(c);
            }
        }
        if (c->state == TCP_FIN_WAIT_1 && seg_ack == c->snd_nxt)
            c->state = TCP_FIN_WAIT_2;
        if (c->state == TCP_CLOSING && seg_ack == c->snd_nxt) {
            c->state         = TCP_TIME_WAIT;
            c->retx_deadline = timer_get_ticks() + TCP_TIME_WAIT_MS;
        }
        if (c->state == TCP_LAST_ACK && seg_ack == c->snd_nxt) {
            retx_clear(c);
            if (c->on_close) c->on_close((int)(c - conns));
            c->state = TCP_CLOSED;
            return;
        }
    }

    /* Data processing */
    if (data_len > 0 && (c->state == TCP_ESTABLISHED ||
                          c->state == TCP_FIN_WAIT_1  ||
                          c->state == TCP_FIN_WAIT_2)) {
        if (seg_seq == c->rcv_nxt) {
            u16 accept = data_len;
            u16 free   = rx_free(c);
            if (accept > free) accept = free;
            if (accept > 0) {
                rx_push(c, data, accept);
                c->rcv_nxt = (u32)(c->rcv_nxt + accept);
                if (c->on_data)
                    c->on_data((int)(c - conns), data, accept);
            }
            send_ctrl(c, TCP_ACK, c->snd_nxt, c->rcv_nxt);
        } else {
            /* Out-of-order: send duplicate ACK */
            send_ctrl(c, TCP_ACK, c->snd_nxt, c->rcv_nxt);
        }
    }

    /* FIN processing */
    if (flags & TCP_FIN) {
        c->rcv_nxt++;   /* FIN consumes one sequence number */
        if (c->state == TCP_ESTABLISHED) {
            c->state = TCP_CLOSE_WAIT;
            send_ctrl(c, TCP_ACK, c->snd_nxt, c->rcv_nxt);
            if (c->on_close) c->on_close((int)(c - conns));
        } else if (c->state == TCP_FIN_WAIT_1) {
            c->state = TCP_CLOSING;
            send_ctrl(c, TCP_ACK, c->snd_nxt, c->rcv_nxt);
        } else if (c->state == TCP_FIN_WAIT_2) {
            c->state         = TCP_TIME_WAIT;
            c->retx_deadline = timer_get_ticks() + TCP_TIME_WAIT_MS;
            send_ctrl(c, TCP_ACK, c->snd_nxt, c->rcv_nxt);
        }
    }
}

/* ── tcp_listen ──────────────────────────────────────────────────────── */
tcp_conn_id_t tcp_listen(u16 local_port,
                         tcp_data_cb_t  on_data,
                         tcp_close_cb_t on_close) {
    int id = conn_alloc();
    if (id < 0) return -1;
    tcp_conn_t *c   = &conns[id];
    c->state        = TCP_LISTEN;
    c->local_ip     = ipv4_get_addr();
    c->local_port   = local_port;
    c->remote_ip    = 0;
    c->remote_port  = 0;
    c->rx_head      = 0;
    c->rx_tail      = 0;
    c->retx_buf     = (void *)0;
    c->retx_len     = 0;
    c->on_data      = on_data;
    c->on_close     = on_close;
    klog(LOG_INFO, "[tcp] listening on port %d (id=%d)\n",
         (int)local_port, id);
    return id;
}

/* ── tcp_connect ─────────────────────────────────────────────────────── */
tcp_conn_id_t tcp_connect(u32 remote_ip, u16 remote_port,
                          u16 local_port,
                          tcp_data_cb_t  on_data,
                          tcp_close_cb_t on_close) {
    int id = conn_alloc();
    if (id < 0) return -1;
    tcp_conn_t *c   = &conns[id];
    c->state        = TCP_SYN_SENT;
    c->local_ip     = ipv4_get_addr();
    c->local_port   = local_port;
    c->remote_ip    = remote_ip;
    c->remote_port  = remote_port;
    c->snd_una      = next_isn();
    c->snd_nxt      = c->snd_una + 1;
    c->snd_wnd      = 0;
    c->rcv_nxt      = 0;
    c->rx_head      = 0;
    c->rx_tail      = 0;
    c->retx_buf     = (void *)0;
    c->retx_len     = 0;
    c->on_data      = on_data;
    c->on_close     = on_close;

    u8 syn[TCP_HDR_LEN];
    tcp_hdr_t *sh = (tcp_hdr_t *)syn;
    sh->src_port = htons(local_port);
    sh->dst_port = htons(remote_port);
    sh->seq      = htonl(c->snd_una);
    sh->ack      = 0;
    sh->data_off = TCP_DATA_OFF_MIN;
    sh->flags    = TCP_SYN;
    sh->window   = htons(TCP_WINDOW);
    sh->checksum = 0;
    sh->urg_ptr  = 0;
    retx_arm(c, syn, TCP_HDR_LEN);
    tcp_send_raw(c, syn, TCP_HDR_LEN);
    klog(LOG_DEBUG, "[tcp] SYN_SENT :%d -> port %d\n",
         (int)local_port, (int)remote_port);
    return id;
}

/* ── tcp_send ────────────────────────────────────────────────────────── */
int tcp_send(tcp_conn_id_t id, const u8 *data, u16 len) {
    if (id < 0 || id >= TCP_MAX_CONN) return -1;
    tcp_conn_t *c = &conns[id];
    if (c->state != TCP_ESTABLISHED && c->state != TCP_CLOSE_WAIT) return -1;
    if (len == 0) return 0;

    u16  total = (u16)(TCP_HDR_LEN + len);
    u8  *seg   = kmalloc(total);
    if (!seg) return -1;

    tcp_hdr_t *h = (tcp_hdr_t *)seg;
    h->src_port = htons(c->local_port);
    h->dst_port = htons(c->remote_port);
    h->seq      = htonl(c->snd_nxt);
    h->ack      = htonl(c->rcv_nxt);
    h->data_off = TCP_DATA_OFF_MIN;
    h->flags    = TCP_PSH | TCP_ACK;
    h->window   = htons(TCP_WINDOW);
    h->checksum = 0;
    h->urg_ptr  = 0;
    memcpy(seg + TCP_HDR_LEN, data, len);

    c->snd_nxt = (u32)(c->snd_nxt + len);
    retx_arm(c, seg, total);
    int ret = tcp_send_raw(c, seg, total);
    kfree(seg);
    return ret;
}

/* ── tcp_close ───────────────────────────────────────────────────────── */
int tcp_close(tcp_conn_id_t id) {
    if (id < 0 || id >= TCP_MAX_CONN) return -1;
    tcp_conn_t *c = &conns[id];

    if (c->state == TCP_ESTABLISHED || c->state == TCP_SYN_RCVD) {
        send_ctrl(c, TCP_FIN | TCP_ACK, c->snd_nxt, c->rcv_nxt);
        c->snd_nxt++;
        c->state = TCP_FIN_WAIT_1;
    } else if (c->state == TCP_CLOSE_WAIT) {
        send_ctrl(c, TCP_FIN | TCP_ACK, c->snd_nxt, c->rcv_nxt);
        c->snd_nxt++;
        c->state = TCP_LAST_ACK;
    } else if (c->state == TCP_LISTEN || c->state == TCP_SYN_SENT) {
        retx_clear(c);
        c->state = TCP_CLOSED;
    }
    return 0;
}

/* ── tcp_tick — retransmit / TIME_WAIT expiry ────────────────────────── */
void tcp_tick(void) {
    u32 now = timer_get_ticks();
    for (int i = 0; i < TCP_MAX_CONN; i++) {
        tcp_conn_t *c = &conns[i];

        if (c->state == TCP_TIME_WAIT) {
            if ((i32)(now - c->retx_deadline) >= 0) {
                c->state = TCP_CLOSED;
                klog(LOG_DEBUG, "[tcp] TIME_WAIT expired id=%d\n", i);
            }
            continue;
        }

        if (!c->retx_buf || c->retx_len == 0) continue;
        if ((i32)(now - c->retx_deadline) < 0) continue;

        /* Retransmit */
        if (c->retx_count >= TCP_MAX_RETRIES) {
            klog(LOG_WARN, "[tcp] max retries id=%d — aborting\n", i);
            retx_clear(c);
            if (c->on_close) c->on_close(i);
            c->state = TCP_CLOSED;
            continue;
        }
        c->retx_count++;
        c->retx_deadline = now + TCP_RTO_MS * (u32)(1u << c->retx_count);
        klog(LOG_DEBUG, "[tcp] retx id=%d attempt=%d\n", i, c->retx_count);
        tcp_send_raw(c, c->retx_buf, c->retx_len);
    }
}

/* ── Module init / dump ──────────────────────────────────────────────── */
static int tcp_init_impl(void) {
    for (int i = 0; i < TCP_MAX_CONN; i++)
        conns[i].state = TCP_CLOSED;
    ipv4_register(IPPROTO_TCP, tcp_rx);
    klog(LOG_INFO, "[tcp] ready (%d slots)\n", TCP_MAX_CONN);
    return 0;
}

static void tcp_dump(void) {
    int active = 0;
    for (int i = 0; i < TCP_MAX_CONN; i++)
        if (conns[i].state != TCP_CLOSED) active++;
    klog(LOG_INFO, "[tcp] %d/%d connections active\n", active, TCP_MAX_CONN);
}

/* ── Module descriptor ───────────────────────────────────────────────── */
kernel_module_t mod_tcp = {
    .name        = "tcp",
    .init        = tcp_init_impl,
    .dump        = tcp_dump,
    .initialized = false,
};

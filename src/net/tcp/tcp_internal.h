/* tcp_internal.h — private constants, on-wire structures, and TCB for TCP */
#ifndef TCP_INTERNAL_H
#define TCP_INTERNAL_H

#include "../../lib/types.h"

/* ── TCP header (20 bytes, no options) ─────────────────────────────── */
typedef struct __attribute__((packed)) {
    u16 src_port;
    u16 dst_port;
    u32 seq;        /* sequence number */
    u32 ack;        /* acknowledgement number */
    u8  data_off;   /* data offset (high 4 bits, in 32-bit words) */
    u8  flags;      /* control bits */
    u16 window;     /* receive window */
    u16 checksum;
    u16 urg_ptr;    /* urgent pointer (unused) */
} tcp_hdr_t;

#define TCP_HDR_LEN     20

/* ── Flag bits ──────────────────────────────────────────────────────── */
#define TCP_FIN     0x01
#define TCP_SYN     0x02
#define TCP_RST     0x04
#define TCP_PSH     0x08
#define TCP_ACK     0x10
#define TCP_URG     0x20

/* ── Data offset field — we always use 20-byte header ───────────────── */
#define TCP_DATA_OFF_MIN    0x50    /* 5 words << 4 */

/* ── Connection states ──────────────────────────────────────────────── */
typedef enum {
    TCP_CLOSED      = 0,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RCVD,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT,
} tcp_state_t;

/* ── Receive buffer ──────────────────────────────────────────────────── */
#define TCP_RX_BUF_SIZE     4096

/* ── Retransmit ──────────────────────────────────────────────────────── */
#define TCP_RTO_MS          1000    /* retransmit timeout in milliseconds */
#define TCP_MAX_RETRIES     5
#define TCP_TIME_WAIT_MS    4000    /* 2×MSL */

/* ── Window advertised to peer ───────────────────────────────────────── */
#define TCP_WINDOW          TCP_RX_BUF_SIZE

/* ── Max simultaneous connections ────────────────────────────────────── */
#define TCP_MAX_CONN        8

/* ── TCP connection control block ────────────────────────────────────── */
typedef struct {
    tcp_state_t state;

    u32  local_ip;
    u16  local_port;
    u32  remote_ip;
    u16  remote_port;

    /* Send sequence space */
    u32  snd_una;       /* oldest unacknowledged byte */
    u32  snd_nxt;       /* next sequence number to send */
    u16  snd_wnd;       /* peer's advertised receive window */

    /* Receive sequence space */
    u32  rcv_nxt;       /* next expected byte from peer */

    /* Retransmit state */
    u8  *retx_buf;      /* copy of unacknowledged segment */
    u16  retx_len;      /* length of retx_buf (TCP header + data) */
    u32  retx_deadline; /* timer tick when retransmit fires */
    int  retx_count;    /* consecutive retransmit attempts */

    /* Receive buffer (simple linear, no reordering) */
    u8   rx_buf[TCP_RX_BUF_SIZE];
    u16  rx_head;       /* read index */
    u16  rx_tail;       /* write index */

    /* Callback for received data (may be NULL) */
    void (*on_data)(int conn_id, const u8 *data, u16 len);
    void (*on_close)(int conn_id);
} tcp_conn_t;

#endif /* TCP_INTERNAL_H */

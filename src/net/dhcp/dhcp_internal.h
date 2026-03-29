/* dhcp_internal.h — private constants and on-wire structures for DHCP */
#ifndef DHCP_INTERNAL_H
#define DHCP_INTERNAL_H

#include "../../lib/types.h"

/* ── DHCP ports ─────────────────────────────────────────────────────── */
#define DHCP_CLIENT_PORT    68
#define DHCP_SERVER_PORT    67

/* ── BOOTP op codes ─────────────────────────────────────────────────── */
#define DHCP_OP_REQUEST     1
#define DHCP_OP_REPLY       2

/* ── DHCP message types (option 53) ─────────────────────────────────── */
#define DHCP_DISCOVER       1
#define DHCP_OFFER          2
#define DHCP_REQUEST        3
#define DHCP_DECLINE        4
#define DHCP_ACK            5
#define DHCP_NAK            6
#define DHCP_RELEASE        7
#define DHCP_INFORM         8

/* ── DHCP option codes ──────────────────────────────────────────────── */
#define DHCP_OPT_SUBNET     1
#define DHCP_OPT_ROUTER     3
#define DHCP_OPT_DNS        6
#define DHCP_OPT_HOSTNAME   12
#define DHCP_OPT_REQIP      50
#define DHCP_OPT_LEASE      51
#define DHCP_OPT_MSGTYPE    53
#define DHCP_OPT_SERVERID   54
#define DHCP_OPT_PARAMREQ   55
#define DHCP_OPT_END        255

/* ── Magic cookie ───────────────────────────────────────────────────── */
#define DHCP_MAGIC_COOKIE   0x63825363u

/* ── BOOTP/DHCP fixed header (236 bytes) ─────────────────────────────── */
typedef struct __attribute__((packed)) {
    u8  op;             /* message type: 1=request, 2=reply */
    u8  htype;          /* hardware type: 1=Ethernet */
    u8  hlen;           /* hardware address length: 6 */
    u8  hops;           /* client sets to 0 */
    u32 xid;            /* transaction ID */
    u16 secs;           /* seconds elapsed */
    u16 flags;          /* broadcast flag */
    u32 ciaddr;         /* client IP (0 if unknown) */
    u32 yiaddr;         /* 'your' IP (assigned by server) */
    u32 siaddr;         /* next server IP */
    u32 giaddr;         /* relay agent IP */
    u8  chaddr[16];     /* client hardware address */
    u8  sname[64];      /* server host name */
    u8  file[128];      /* boot file name */
    u32 magic;          /* DHCP magic cookie */
} dhcp_hdr_t;

#define DHCP_HDR_LEN        240     /* fixed header size including magic cookie */
#define DHCP_MIN_SIZE       548     /* libslirp bootp_t minimum: 236+4+308 */
#define DHCP_OPTIONS_LEN    308     /* max options we send/receive */
#define DHCP_PKT_LEN        DHCP_MIN_SIZE

/* ── DHCP client state ──────────────────────────────────────────────── */
typedef enum {
    DHCP_STATE_IDLE = 0,
    DHCP_STATE_SELECTING,   /* sent DISCOVER, waiting for OFFER */
    DHCP_STATE_REQUESTING,  /* sent REQUEST, waiting for ACK */
    DHCP_STATE_BOUND,       /* have IP, lease active */
    DHCP_STATE_FAILED,      /* max retries reached */
} dhcp_state_t;

#define DHCP_XID            0xDEADBEEFu   /* fixed transaction ID */
#define DHCP_MAX_RETRIES    5
#define DHCP_RETRY_MS       3000

#endif /* DHCP_INTERNAL_H */

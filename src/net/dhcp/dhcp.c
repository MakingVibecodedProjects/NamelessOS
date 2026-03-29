#include "dhcp.h"
#include "dhcp_internal.h"
#include "../udp/udp.h"
#include "../ipv4/ipv4.h"
#include "../ethernet/ethernet.h"
#include "../ethernet/ethernet_internal.h"
#include "../tcp/tcp.h"
#include "../../timer/timer.h"
#include "../../serial/serial.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

/* ── Client state ────────────────────────────────────────────────────── */
static dhcp_state_t state       = DHCP_STATE_IDLE;
static u32          offered_ip  = 0;
static u32          server_ip   = 0;
static int          retry_count = 0;
static u32          deadline    = 0;

/* ── Option writer helpers ───────────────────────────────────────────── */
static u8 *opt_u8(u8 *p, u8 code, u8 val) {
    *p++ = code; *p++ = 1; *p++ = val; return p;
}
static u8 *opt_u32(u8 *p, u8 code, u32 val) {
    *p++ = code; *p++ = 4;
    *p++ = (u8)(val >> 24); *p++ = (u8)(val >> 16);
    *p++ = (u8)(val >>  8); *p++ = (u8)(val);
    return p;
}

/* ── Build and send a DHCP packet ────────────────────────────────────── */
static void send_discover(void) {
    u8 pkt[DHCP_PKT_LEN];
    memset(pkt, 0, DHCP_PKT_LEN);

    dhcp_hdr_t *h = (dhcp_hdr_t *)pkt;
    h->op     = DHCP_OP_REQUEST;
    h->htype  = 1;
    h->hlen   = 6;
    h->xid    = htonl(DHCP_XID);
    h->flags  = htons(0x0000);  /* unicast flag — QEMU SLIRP ignores bcast flag */
    h->magic  = htonl(DHCP_MAGIC_COOKIE);

    ethernet_get_mac(h->chaddr);

    /* Options */
    u8 *p = pkt + DHCP_HDR_LEN;
    p = opt_u8(p, DHCP_OPT_MSGTYPE, DHCP_DISCOVER);
    /* Parameter request list */
    *p++ = DHCP_OPT_PARAMREQ; *p++ = 3;
    *p++ = DHCP_OPT_SUBNET; *p++ = DHCP_OPT_ROUTER; *p++ = DHCP_OPT_DNS;
    *p++ = DHCP_OPT_END;

    u16 len = (u16)(p - pkt);
    if (len < DHCP_MIN_SIZE) len = DHCP_MIN_SIZE;
    udp_send(0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, pkt, len);
    klog(LOG_INFO, "[dhcp] DISCOVER sent (xid=0x%x)\n", DHCP_XID);
}

static void send_request(u32 req_ip, u32 srv_ip) {
    u8 pkt[DHCP_PKT_LEN];
    memset(pkt, 0, DHCP_PKT_LEN);

    dhcp_hdr_t *h = (dhcp_hdr_t *)pkt;
    h->op     = DHCP_OP_REQUEST;
    h->htype  = 1;
    h->hlen   = 6;
    h->xid    = htonl(DHCP_XID);
    h->flags  = htons(0x0000);
    h->magic  = htonl(DHCP_MAGIC_COOKIE);

    ethernet_get_mac(h->chaddr);

    u8 *p = pkt + DHCP_HDR_LEN;
    p = opt_u8 (p, DHCP_OPT_MSGTYPE,  DHCP_REQUEST);
    p = opt_u32(p, DHCP_OPT_REQIP,    req_ip);
    p = opt_u32(p, DHCP_OPT_SERVERID, srv_ip);
    *p++ = DHCP_OPT_PARAMREQ; *p++ = 3;
    *p++ = DHCP_OPT_SUBNET; *p++ = DHCP_OPT_ROUTER; *p++ = DHCP_OPT_DNS;
    *p++ = DHCP_OPT_END;

    u16 len = (u16)(p - pkt);
    if (len < DHCP_MIN_SIZE) len = DHCP_MIN_SIZE;
    udp_send(0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, pkt, len);
    klog(LOG_INFO, "[dhcp] REQUEST sent for %d.%d.%d.%d\n",
         (int)(req_ip>>24),(int)((req_ip>>16)&0xFF),
         (int)((req_ip>>8)&0xFF),(int)(req_ip&0xFF));
}

/* ── Parse options, return message type; fill offered_ip/server_ip ─── */
static u8 parse_options(const u8 *opts, u16 len,
                        u32 *out_srv, u32 *out_mask, u32 *out_gw) {
    u8 msgtype = 0;
    const u8 *p   = opts;
    const u8 *end = opts + len;
    while (p < end) {
        u8 code = *p++;
        if (code == DHCP_OPT_END) break;
        if (code == 0) continue;    /* PAD */
        if (p >= end) break;
        u8 olen = *p++;
        if (p + olen > end) break;
        switch (code) {
        case DHCP_OPT_MSGTYPE:
            if (olen >= 1) msgtype = p[0];
            break;
        case DHCP_OPT_SERVERID:
            if (olen >= 4 && out_srv)
                *out_srv = (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|(u32)p[3];
            break;
        case DHCP_OPT_SUBNET:
            if (olen >= 4 && out_mask)
                *out_mask = (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|(u32)p[3];
            break;
        case DHCP_OPT_ROUTER:
            if (olen >= 4 && out_gw)
                *out_gw = (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|(u32)p[3];
            break;
        default:
            break;
        }
        p += olen;
    }
    return msgtype;
}

/* ── UDP receive callback (port 68) ─────────────────────────────────── */
static void dhcp_rx(const u8 *data, u16 len,
                    u32 src_ip, u16 src_port, u16 dst_port) {
    (void)src_ip; (void)src_port; (void)dst_port;
    if (len < DHCP_HDR_LEN) return;

    const dhcp_hdr_t *h = (const dhcp_hdr_t *)data;
    if (h->op    != DHCP_OP_REPLY)        return;
    if (ntohl(h->xid)   != DHCP_XID)     return;
    if (ntohl(h->magic) != DHCP_MAGIC_COOKIE) return;

    u32 yiaddr = ntohl(h->yiaddr);
    if (yiaddr == 0) return;

    const u8 *opts    = data + DHCP_HDR_LEN;
    u16       opts_len = (u16)(len - DHCP_HDR_LEN);

    u32 srv  = 0;
    u32 mask = 0xFFFFFF00u;   /* /24 default */
    u32 gw   = 0;

    u8 msgtype = parse_options(opts, opts_len, &srv, &mask, &gw);

    if (state == DHCP_STATE_SELECTING && msgtype == DHCP_OFFER) {
        offered_ip = yiaddr;
        server_ip  = srv;
        klog(LOG_INFO, "[dhcp] OFFER %d.%d.%d.%d from server %d.%d.%d.%d\n",
             (int)(yiaddr>>24),(int)((yiaddr>>16)&0xFF),
             (int)((yiaddr>>8)&0xFF),(int)(yiaddr&0xFF),
             (int)(srv>>24),(int)((srv>>16)&0xFF),
             (int)((srv>>8)&0xFF),(int)(srv&0xFF));
        send_request(offered_ip, server_ip);
        state    = DHCP_STATE_REQUESTING;
        deadline = timer_get_ticks() + DHCP_RETRY_MS;
    } else if (state == DHCP_STATE_REQUESTING && msgtype == DHCP_ACK) {
        if (gw == 0) gw = server_ip;   /* fall back to server as gateway */
        ipv4_set_addr(yiaddr, mask, gw);
        state = DHCP_STATE_BOUND;
        klog(LOG_INFO, "[dhcp] bound — IP %d.%d.%d.%d mask %d.%d.%d.%d gw %d.%d.%d.%d\n",
             (int)(yiaddr>>24),(int)((yiaddr>>16)&0xFF),
             (int)((yiaddr>>8)&0xFF),(int)(yiaddr&0xFF),
             (int)(mask>>24),(int)((mask>>16)&0xFF),
             (int)((mask>>8)&0xFF),(int)(mask&0xFF),
             (int)(gw>>24),(int)((gw>>16)&0xFF),
             (int)((gw>>8)&0xFF),(int)(gw&0xFF));
    } else if (msgtype == DHCP_NAK) {
        klog(LOG_WARN, "[dhcp] NAK received — restarting\n");
        state       = DHCP_STATE_SELECTING;
        offered_ip  = 0;
        server_ip   = 0;
        retry_count = 0;
        send_discover();
        deadline = timer_get_ticks() + DHCP_RETRY_MS;
    }
}

/* ── dhcp_tick ───────────────────────────────────────────────────────── */
int dhcp_tick(void) {
    if (state == DHCP_STATE_BOUND)   return 1;
    if (state == DHCP_STATE_IDLE)    return 0;
    if (state == DHCP_STATE_FAILED)  return -1;

    u32 now = timer_get_ticks();
    if ((i32)(now - deadline) < 0) return 0;  /* not yet */

    if (retry_count >= DHCP_MAX_RETRIES) {
        klog(LOG_WARN, "[dhcp] max retries — giving up\n");
        state = DHCP_STATE_FAILED;
        return -1;
    }
    retry_count++;
    deadline = now + DHCP_RETRY_MS;

    if (state == DHCP_STATE_SELECTING) {
        klog(LOG_DEBUG, "[dhcp] retry DISCOVER (%d/%d)\n",
             retry_count, DHCP_MAX_RETRIES);
        send_discover();
    } else if (state == DHCP_STATE_REQUESTING) {
        klog(LOG_DEBUG, "[dhcp] retry REQUEST (%d/%d)\n",
             retry_count, DHCP_MAX_RETRIES);
        send_request(offered_ip, server_ip);
    }
    return 0;
}

/* ── dhcp_bound ──────────────────────────────────────────────────────── */
int dhcp_bound(void) {
    return state == DHCP_STATE_BOUND ? 1 : 0;
}

/* ── net_poll — called every timer tick to drain NIC + drive timers ─── */
static void net_poll(void) {
    ethernet_poll();
    dhcp_tick();
    tcp_tick();
}

/* ── Module init / dump ──────────────────────────────────────────────── */
static int dhcp_init_impl(void) {
    state       = DHCP_STATE_IDLE;
    offered_ip  = 0;
    server_ip   = 0;
    retry_count = 0;

    udp_register(DHCP_CLIENT_PORT, dhcp_rx);
    timer_register_callback(net_poll);

    /* Kick off DISCOVER immediately */
    send_discover();
    state    = DHCP_STATE_SELECTING;
    deadline = timer_get_ticks() + DHCP_RETRY_MS;

    klog(LOG_INFO, "[dhcp] client started\n");
    return 0;
}

static void dhcp_dump(void) {
    const char *names[] = {
        "IDLE", "SELECTING", "REQUESTING", "BOUND", "FAILED"
    };
    klog(LOG_INFO, "[dhcp] state=%s ip=%d.%d.%d.%d\n",
         names[state],
         (int)(offered_ip>>24),(int)((offered_ip>>16)&0xFF),
         (int)((offered_ip>>8)&0xFF),(int)(offered_ip&0xFF));
}

/* ── Module descriptor ───────────────────────────────────────────────── */
kernel_module_t mod_dhcp = {
    .name        = "dhcp",
    .init        = dhcp_init_impl,
    .dump        = dhcp_dump,
    .initialized = false,
};

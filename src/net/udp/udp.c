#include "udp.h"
#include "udp_internal.h"
#include "../ipv4/ipv4.h"
#include "../ipv4/ipv4_internal.h"
#include "../ethernet/ethernet_internal.h"
#include "../../heap/heap.h"
#include "../../serial/serial.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

/* ── Handler table ───────────────────────────────────────────────────── */
typedef struct {
    u16              port;      /* host byte order; 0 = catch-all */
    udp_rx_handler_t handler;
} udp_entry_t;

static udp_entry_t handlers[UDP_MAX_HANDLERS];
static int         handler_count = 0;

/* ── csum_add — add a byte buffer to a running checksum accumulator ─── */
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

/* ── udp_checksum ────────────────────────────────────────────────────── */
/* Compute UDP checksum over pseudo-header + UDP header + data.
   Returns 0xFFFF if the one's-complement result is 0. */
static u16 udp_checksum(u32 src_ip, u32 dst_ip,
                        const udp_hdr_t *hdr, const u8 *data, u16 data_len) {
    u16 udp_len = (u16)(UDP_HDR_LEN + data_len);

    /* Build pseudo-header as a plain byte array to avoid packed-pointer cast */
    u8 ph[12];
    ph[0]  = (u8)(src_ip >> 24); ph[1]  = (u8)(src_ip >> 16);
    ph[2]  = (u8)(src_ip >>  8); ph[3]  = (u8)(src_ip);
    ph[4]  = (u8)(dst_ip >> 24); ph[5]  = (u8)(dst_ip >> 16);
    ph[6]  = (u8)(dst_ip >>  8); ph[7]  = (u8)(dst_ip);
    ph[8]  = 0;
    ph[9]  = IPPROTO_UDP;
    ph[10] = (u8)(udp_len >> 8); ph[11] = (u8)(udp_len);

    u32 sum = 0;
    sum = csum_add(sum, ph, 12);
    sum = csum_add(sum, (const u8 *)hdr, UDP_HDR_LEN);
    if (data && data_len)
        sum = csum_add(sum, data, data_len);

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    u16 result = (u16)~sum;
    return result ? result : 0xFFFF;
}

/* ── udp_rx — registered as IPv4 UDP handler ────────────────────────── */
static void udp_rx(const u8 *payload, u16 len,
                   u32 src_ip, u32 dst_ip) {
    (void)dst_ip;
    if (len < UDP_HDR_LEN) return;

    const udp_hdr_t *hdr = (const udp_hdr_t *)payload;
    u16 src_port  = ntohs(hdr->src_port);
    u16 dst_port  = ntohs(hdr->dst_port);
    u16 udp_len   = ntohs(hdr->length);

    if (udp_len < UDP_HDR_LEN || udp_len > len) return;

    u16       data_len = (u16)(udp_len - UDP_HDR_LEN);
    const u8 *data     = payload + UDP_HDR_LEN;

    /* Dispatch: first exact port match, then catch-all (port 0) */
    udp_rx_handler_t catch_all = (void *)0;
    for (int i = 0; i < handler_count; i++) {
        if (handlers[i].port == dst_port) {
            handlers[i].handler(data, data_len, src_ip, src_port, dst_port);
            return;
        }
        if (handlers[i].port == 0)
            catch_all = handlers[i].handler;
    }
    if (catch_all) {
        catch_all(data, data_len, src_ip, src_port, dst_port);
        return;
    }

    klog(LOG_DEBUG, "[udp] no handler for port %d — dropped\n", (int)dst_port);
}

/* ── udp_register ────────────────────────────────────────────────────── */
int udp_register(u16 dst_port, udp_rx_handler_t handler) {
    if (handler_count >= UDP_MAX_HANDLERS) return -1;
    handlers[handler_count].port    = dst_port;
    handlers[handler_count].handler = handler;
    handler_count++;
    return 0;
}

/* ── udp_send ────────────────────────────────────────────────────────── */
int udp_send(u32 dst_ip, u16 src_port, u16 dst_port,
             const u8 *data, u16 len) {
    u16  total = (u16)(UDP_HDR_LEN + len);
    u8  *pkt   = kmalloc(total);
    if (!pkt) return -1;

    udp_hdr_t *hdr = (udp_hdr_t *)pkt;
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length   = htons(total);
    hdr->checksum = 0;

    if (data && len)
        memcpy(pkt + UDP_HDR_LEN, data, len);

    u32 src_ip = ipv4_get_addr();
    hdr->checksum = udp_checksum(src_ip, dst_ip, hdr,
                                 pkt + UDP_HDR_LEN, len);

    int ret = ipv4_send(dst_ip, IPPROTO_UDP, pkt, total);
    kfree(pkt);
    return ret;
}

/* ── Module init / dump ──────────────────────────────────────────────── */
static int udp_init_impl(void) {
    ipv4_register(IPPROTO_UDP, udp_rx);
    klog(LOG_INFO, "[udp] ready\n");
    return 0;
}

static void udp_dump(void) {
    klog(LOG_INFO, "[udp] %d port handler(s) registered\n", handler_count);
}

/* ── Module descriptor ───────────────────────────────────────────────── */
kernel_module_t mod_udp = {
    .name        = "udp",
    .init        = udp_init_impl,
    .dump        = udp_dump,
    .initialized = false,
};

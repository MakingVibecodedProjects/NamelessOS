#include "icmp.h"
#include "icmp_internal.h"
#include "../ipv4/ipv4.h"
#include "../ipv4/ipv4_internal.h"
#include "../ethernet/ethernet_internal.h"
#include "../../heap/heap.h"
#include "../../serial/serial.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

/* ── icmp_rx — registered as IPv4 ICMP handler ──────────────────────── */
static void icmp_rx(const u8 *payload, u16 len,
                    u32 src_ip, u32 dst_ip) {
    (void)dst_ip;
    if (len < ICMP_HDR_LEN) return;

    const icmp_hdr_t *hdr = (const icmp_hdr_t *)payload;

    if (hdr->type == ICMP_TYPE_ECHO_REQUEST && hdr->code == 0) {
        /* Build echo reply — same payload, swap type to 0 */
        u8 *reply = kmalloc(len);
        if (!reply) return;
        memcpy(reply, payload, len);

        icmp_hdr_t *rhdr = (icmp_hdr_t *)reply;
        rhdr->type     = ICMP_TYPE_ECHO_REPLY;
        rhdr->checksum = 0;
        rhdr->checksum = ip_checksum(reply, len);

        ipv4_send(src_ip, IPPROTO_ICMP, reply, len);
        kfree(reply);

        klog(LOG_INFO, "[icmp] echo reply → %d.%d.%d.%d id=%x seq=%d\n",
             (int)(src_ip >> 24), (int)((src_ip >> 16) & 0xFF),
             (int)((src_ip >>  8) & 0xFF), (int)(src_ip & 0xFF),
             (unsigned)ntohs(hdr->id), (int)ntohs(hdr->seq));
    }
}

/* ── icmp_ping ───────────────────────────────────────────────────────── */
int icmp_ping(u32 dst_ip, u16 id, u16 seq, const u8 *data, u16 len) {
    u16 total = (u16)(ICMP_HDR_LEN + len);
    u8 *pkt = kmalloc(total);
    if (!pkt) return -1;

    icmp_hdr_t *hdr = (icmp_hdr_t *)pkt;
    hdr->type     = ICMP_TYPE_ECHO_REQUEST;
    hdr->code     = 0;
    hdr->checksum = 0;
    hdr->id       = htons(id);
    hdr->seq      = htons(seq);

    if (data && len)
        memcpy(pkt + ICMP_HDR_LEN, data, len);

    hdr->checksum = ip_checksum(pkt, total);

    int ret = ipv4_send(dst_ip, IPPROTO_ICMP, pkt, total);
    kfree(pkt);
    return ret;
}

/* ── Module init / dump ──────────────────────────────────────────────── */
static int icmp_init_impl(void) {
    ipv4_register(IPPROTO_ICMP, icmp_rx);
    klog(LOG_INFO, "[icmp] ready\n");
    return 0;
}

static void icmp_dump(void) {
    klog(LOG_INFO, "[icmp] echo request/reply handler active\n");
}

/* ── Module descriptor ───────────────────────────────────────────────── */
kernel_module_t mod_icmp = {
    .name        = "icmp",
    .init        = icmp_init_impl,
    .dump        = icmp_dump,
    .initialized = false,
};

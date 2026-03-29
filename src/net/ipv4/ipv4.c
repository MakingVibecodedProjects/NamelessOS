#include "ipv4.h"
#include "ipv4_internal.h"
#include "../ethernet/ethernet.h"
#include "../ethernet/ethernet_internal.h"
#include "../arp/arp.h"
#include "../../serial/serial.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

/* ── Protocol handler table ─────────────────────────────────────────── */
typedef struct {
    u8                proto;
    ipv4_rx_handler_t handler;
} ipv4_entry_t;

static ipv4_entry_t handlers[IPV4_MAX_HANDLERS];
static u32          handler_count = 0;

/* ── Our IPv4 configuration ─────────────────────────────────────────── */
static u32 our_ip      = 0;
static u32 our_netmask = 0;
static u32 our_gateway = 0;

/* ── Packet ID counter ──────────────────────────────────────────────── */
static u16 pkt_id = 1;

/* ── ipv4_rx — registered as Ethernet IPv4 handler ─────────────────── */
static void ipv4_rx(const u8 *payload, u16 len, const u8 src_mac[6]) {
    (void)src_mac;
    if (len < IP_HDR_LEN) return;

    const ip_hdr_t *hdr = (const ip_hdr_t *)payload;

    /* Validate version and IHL */
    if ((hdr->ver_ihl >> 4) != IP_VERSION) return;
    u8 ihl = (u8)((hdr->ver_ihl & 0x0F) * 4);
    if (ihl < IP_HDR_LEN || ihl > len) return;

    /* Validate total length */
    u16 total = ntohs(hdr->total_len);
    if (total < ihl || total > len) return;

    /* Drop fragments (we don't reassemble) */
    u16 frag = ntohs(hdr->frag_off);
    if ((frag & 0x1FFF) != 0) return;   /* non-zero fragment offset */
    if (frag & 0x2000)        return;   /* MF flag set */

    u32 src = ntohl(hdr->src);
    u32 dst = ntohl(hdr->dst);

    /* Accept unicast to our IP, or broadcast, or if not configured yet */
    if (our_ip != 0) {
        u32 bcast = (our_ip & our_netmask) | ~our_netmask;
        if (dst != our_ip && dst != bcast && dst != 0xFFFFFFFF) return;
    }

    u16 payload_len = (u16)(total - ihl);
    const u8 *proto_payload = payload + ihl;

    for (u32 i = 0; i < handler_count; i++) {
        if (handlers[i].proto == hdr->proto) {
            handlers[i].handler(proto_payload, payload_len, src, dst);
            return;
        }
    }
}

/* ── Public API ─────────────────────────────────────────────────────── */

int ipv4_register(u8 proto, ipv4_rx_handler_t handler) {
    if (handler_count >= IPV4_MAX_HANDLERS) return -1;
    handlers[handler_count].proto   = proto;
    handlers[handler_count].handler = handler;
    handler_count++;
    return 0;
}

void ipv4_set_addr(u32 ip, u32 netmask, u32 gateway) {
    our_ip      = ip;
    our_netmask = netmask;
    our_gateway = gateway;

    /* Notify ARP layer so it can reply to who-has requests */
    arp_set_ip(ip);

    klog(LOG_INFO, "[ipv4] address set: %d.%d.%d.%d/%d.%d.%d.%d gw %d.%d.%d.%d\n",
         (int)(ip >> 24),      (int)((ip >> 16) & 0xFF),
         (int)((ip >> 8)&0xFF),(int)(ip & 0xFF),
         (int)(netmask >> 24), (int)((netmask >> 16) & 0xFF),
         (int)((netmask >> 8) & 0xFF), (int)(netmask & 0xFF),
         (int)(gateway >> 24), (int)((gateway >> 16) & 0xFF),
         (int)((gateway >> 8) & 0xFF), (int)(gateway & 0xFF));
}

u32 ipv4_get_addr(void) {
    return our_ip;
}

int ipv4_send(u32 dst_ip, u8 proto, const u8 *payload, u16 len) {
    if (len > (u16)(1500 - IP_HDR_LEN)) return -1;

    /* Resolve next-hop MAC via ARP */
    u32 next_hop = dst_ip;
    if (our_ip && our_netmask) {
        /* Off-link: route via gateway */
        if ((dst_ip & our_netmask) != (our_ip & our_netmask))
            next_hop = our_gateway;
    }

    u8 dst_mac[6];
    if (next_hop == 0xFFFFFFFF || dst_ip == 0xFFFFFFFF) {
        /* Limited broadcast — use Ethernet broadcast MAC, no ARP needed */
        dst_mac[0] = dst_mac[1] = dst_mac[2] = 0xFF;
        dst_mac[3] = dst_mac[4] = dst_mac[5] = 0xFF;
    } else if (arp_lookup(next_hop, dst_mac) < 0) {
        /* MAC unknown — send ARP request and drop this packet */
        arp_request(next_hop);
        return -1;
    }

    /* Build IPv4 header */
    u8 frame[IP_HDR_LEN + 1480];   /* max payload for standard MTU */
    ip_hdr_t *hdr = (ip_hdr_t *)frame;

    hdr->ver_ihl   = IP_VER_IHL;
    hdr->dscp_ecn  = 0;
    hdr->total_len = htons((u16)(IP_HDR_LEN + len));
    hdr->id        = htons(pkt_id++);
    hdr->frag_off  = htons(IP_FLAG_DF);
    hdr->ttl       = IP_DEFAULT_TTL;
    hdr->proto     = proto;
    hdr->checksum  = 0;
    hdr->src       = htonl(our_ip);
    hdr->dst       = htonl(dst_ip);
    hdr->checksum  = ip_checksum(hdr, IP_HDR_LEN);

    memcpy(frame + IP_HDR_LEN, payload, len);

    return ethernet_send(dst_mac, ETHERTYPE_IPV4,
                         frame, (u16)(IP_HDR_LEN + len));
}

/* ── Module init / dump ──────────────────────────────────────────────── */
static int ipv4_init_impl(void) {
    handler_count = 0;
    our_ip = our_netmask = our_gateway = 0;
    pkt_id = 1;

    ethernet_register(ETHERTYPE_IPV4, ipv4_rx);

    klog(LOG_INFO, "[ipv4] ready\n");
    return 0;
}

static void ipv4_dump(void) {
    klog(LOG_INFO, "[ipv4] addr=%d.%d.%d.%d handlers=%u\n",
         (int)(our_ip >> 24), (int)((our_ip >> 16) & 0xFF),
         (int)((our_ip >>  8) & 0xFF), (int)(our_ip & 0xFF),
         (unsigned)handler_count);
}

/* ── Module descriptor ───────────────────────────────────────────────── */
kernel_module_t mod_ipv4 = {
    .name        = "ipv4",
    .init        = ipv4_init_impl,
    .dump        = ipv4_dump,
    .initialized = false,
};

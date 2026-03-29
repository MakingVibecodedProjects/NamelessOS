#include "arp.h"
#include "arp_internal.h"
#include "../ethernet/ethernet.h"
#include "../ethernet/ethernet_internal.h"
#include "../../serial/serial.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

/* ── ARP table ─────────────────────────────────────────────────────── */
static arp_entry_t table[ARP_TABLE_SIZE];

/* ── Our IPv4 address (set by IPv4/DHCP layer when we have one) ────── */
static u32 our_ip = 0;   /* host byte order; 0 = not configured yet */

/* ── arp_lookup ─────────────────────────────────────────────────────── */
int arp_lookup(u32 ip, u8 mac[6]) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (table[i].ip == ip) {
            memcpy(mac, table[i].mac, 6);
            return 0;
        }
    }
    return -1;
}

/* ── arp_insert ─────────────────────────────────────────────────────── */
void arp_insert(u32 ip, const u8 mac[6]) {
    /* Update existing entry if present */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (table[i].ip == ip) {
            memcpy(table[i].mac, mac, 6);
            return;
        }
    }
    /* Find an empty slot */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (table[i].ip == 0) {
            table[i].ip = ip;
            memcpy(table[i].mac, mac, 6);
            return;
        }
    }
    /* Table full — overwrite slot 0 (simple eviction) */
    table[0].ip = ip;
    memcpy(table[0].mac, mac, 6);
}

/* ── arp_request ────────────────────────────────────────────────────── */
void arp_request(u32 ip) {
    u8 our_mac[6];
    ethernet_get_mac(our_mac);

    arp_pkt_t pkt;
    pkt.htype = htons(ARP_HTYPE_ETHERNET);
    pkt.ptype = htons(ARP_PTYPE_IPV4);
    pkt.hlen  = 6;
    pkt.plen  = 4;
    pkt.oper  = htons(ARP_OP_REQUEST);

    memcpy(pkt.sha, our_mac, 6);
    /* spa: our IPv4 in network byte order */
    u32 spa_net = htonl(our_ip);
    memcpy(pkt.spa, &spa_net, 4);

    memset(pkt.tha, 0, 6);
    u32 tpa_net = htonl(ip);
    memcpy(pkt.tpa, &tpa_net, 4);

    static const u8 broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    ethernet_send(broadcast, ETHERTYPE_ARP, (const u8 *)&pkt, sizeof(pkt));
}

/* ── arp_rx — registered as Ethernet ARP handler ────────────────────── */
static void arp_rx(const u8 *payload, u16 len, const u8 src_mac[6]) {
    (void)src_mac;
    if (len < (u16)sizeof(arp_pkt_t)) return;

    const arp_pkt_t *pkt = (const arp_pkt_t *)payload;

    /* Only handle Ethernet/IPv4 ARP */
    if (ntohs(pkt->htype) != ARP_HTYPE_ETHERNET) return;
    if (ntohs(pkt->ptype) != ARP_PTYPE_IPV4)     return;
    if (pkt->hlen != 6 || pkt->plen != 4)        return;

    u16 oper = ntohs(pkt->oper);

    /* Learn sender's mapping unconditionally */
    u32 sender_ip;
    memcpy(&sender_ip, pkt->spa, 4);
    sender_ip = ntohl(sender_ip);
    if (sender_ip != 0)
        arp_insert(sender_ip, pkt->sha);

    if (oper == ARP_OP_REQUEST && our_ip != 0) {
        /* Is the request for our IP? */
        u32 target_ip;
        memcpy(&target_ip, pkt->tpa, 4);
        target_ip = ntohl(target_ip);
        if (target_ip != our_ip) return;

        /* Send ARP reply */
        u8 our_mac[6];
        ethernet_get_mac(our_mac);

        arp_pkt_t reply;
        reply.htype = htons(ARP_HTYPE_ETHERNET);
        reply.ptype = htons(ARP_PTYPE_IPV4);
        reply.hlen  = 6;
        reply.plen  = 4;
        reply.oper  = htons(ARP_OP_REPLY);

        memcpy(reply.sha, our_mac, 6);
        u32 spa_net = htonl(our_ip);
        memcpy(reply.spa, &spa_net, 4);
        memcpy(reply.tha, pkt->sha, 6);
        memcpy(reply.tpa, pkt->spa, 4);   /* already network byte order */

        ethernet_send(pkt->sha, ETHERTYPE_ARP,
                      (const u8 *)&reply, sizeof(reply));

        klog(LOG_INFO, "[arp] replied to who-has %d.%d.%d.%d\n",
             (int)(our_ip >> 24), (int)((our_ip >> 16) & 0xFF),
             (int)((our_ip >> 8) & 0xFF), (int)(our_ip & 0xFF));
    }
}

/* ── arp_set_ip ─────────────────────────────────────────────────────── */
void arp_set_ip(u32 ip) {
    our_ip = ip;
}

/* ── Module init / dump ─────────────────────────────────────────────── */
static int arp_init_impl(void) {
    memset(table, 0, sizeof(table));
    our_ip = 0;

    ethernet_register(ETHERTYPE_ARP, arp_rx);

    klog(LOG_INFO, "[arp] ready (%d-entry table)\n", ARP_TABLE_SIZE);
    return 0;
}

static void arp_dump(void) {
    int count = 0;
    for (int i = 0; i < ARP_TABLE_SIZE; i++)
        if (table[i].ip) count++;
    klog(LOG_INFO, "[arp] %d entries\n", count);
}

/* ── Module descriptor ─────────────────────────────────────────────── */
kernel_module_t mod_arp = {
    .name        = "arp",
    .init        = arp_init_impl,
    .dump        = arp_dump,
    .initialized = false,
};

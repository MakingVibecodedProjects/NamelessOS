/* arp_internal.h — private constants and on-wire structures for ARP */
#ifndef ARP_INTERNAL_H
#define ARP_INTERNAL_H

#include "../../lib/types.h"

/* ── ARP hardware / protocol types ────────────────────────────────── */
#define ARP_HTYPE_ETHERNET  1
#define ARP_PTYPE_IPV4      0x0800

/* ── ARP operation codes ───────────────────────────────────────────── */
#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

/* ── ARP packet for Ethernet/IPv4 (on-wire, big-endian, 28 bytes) ─── */
typedef struct __attribute__((packed)) {
    u16 htype;      /* Hardware type: 1 = Ethernet */
    u16 ptype;      /* Protocol type: 0x0800 = IPv4 */
    u8  hlen;       /* Hardware address length: 6 */
    u8  plen;       /* Protocol address length: 4 */
    u16 oper;       /* Operation: 1=request, 2=reply */
    u8  sha[6];     /* Sender hardware address (MAC) */
    u8  spa[4];     /* Sender protocol address (IPv4) */
    u8  tha[6];     /* Target hardware address (MAC) */
    u8  tpa[4];     /* Target protocol address (IPv4) */
} arp_pkt_t;

/* ── ARP table ─────────────────────────────────────────────────────── */
#define ARP_TABLE_SIZE  16

typedef struct {
    u32 ip;         /* IPv4 address (host byte order), 0 = empty slot */
    u8  mac[6];     /* Resolved MAC address */
} arp_entry_t;

#endif /* ARP_INTERNAL_H */

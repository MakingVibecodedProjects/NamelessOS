/* ethernet_internal.h — private constants for the Ethernet layer */
#ifndef ETHERNET_INTERNAL_H
#define ETHERNET_INTERNAL_H

#include "../../lib/types.h"

/* ── EtherTypes ────────────────────────────────────────────────────── */
#define ETH_TYPE_IPV4   0x0800
#define ETH_TYPE_ARP    0x0806
#define ETH_TYPE_IPV6   0x86DD

/* ── Frame size limits ─────────────────────────────────────────────── */
#define ETH_MIN_PAYLOAD  46     /* minimum payload (padding to 64-byte frame) */
#define ETH_MAX_PAYLOAD  1500   /* maximum payload (MTU) */
#define ETH_HEADER_LEN   14     /* dst(6) + src(6) + type(2) */
#define ETH_MAX_FRAME    (ETH_HEADER_LEN + ETH_MAX_PAYLOAD)

/* ── Ethernet header (on-wire layout, big-endian) ──────────────────── */
typedef struct __attribute__((packed)) {
    u8  dst[6];
    u8  src[6];
    u16 type;   /* EtherType in network byte order */
} eth_hdr_t;

/* ── Byte-order helpers (host is little-endian x86_64) ─────────────── */
static inline u16 htons(u16 v) {
    return (u16)((v >> 8) | (v << 8));
}
static inline u16 ntohs(u16 v) {
    return (u16)((v >> 8) | (v << 8));
}
static inline u32 htonl(u32 v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0xFF000000u) >> 24);
}
static inline u32 ntohl(u32 v) { return htonl(v); }

/* Maximum number of ethertype handlers */
#define ETH_MAX_HANDLERS 8

#endif /* ETHERNET_INTERNAL_H */

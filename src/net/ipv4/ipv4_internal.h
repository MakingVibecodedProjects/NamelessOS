/* ipv4_internal.h — private constants and on-wire structures for IPv4 */
#ifndef IPV4_INTERNAL_H
#define IPV4_INTERNAL_H

#include "../../lib/types.h"

/* ── IP protocol numbers ───────────────────────────────────────────── */
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

/* ── IPv4 header (20 bytes, no options) ────────────────────────────── */
typedef struct __attribute__((packed)) {
    u8  ver_ihl;    /* version(4) | IHL(4) — always 0x45 for us */
    u8  dscp_ecn;   /* differentiated services, always 0 */
    u16 total_len;  /* header + payload, network byte order */
    u16 id;         /* fragment identification */
    u16 frag_off;   /* flags(3) | fragment offset(13) */
    u8  ttl;        /* time to live */
    u8  proto;      /* protocol number */
    u16 checksum;   /* header checksum (0 = not yet computed) */
    u32 src;        /* source IP, network byte order */
    u32 dst;        /* destination IP, network byte order */
} ip_hdr_t;

#define IP_VERSION      4
#define IP_IHL_WORDS    5           /* 5 × 32-bit words = 20 bytes */
#define IP_VER_IHL      0x45        /* version=4, IHL=5 */
#define IP_DEFAULT_TTL  64
#define IP_HDR_LEN      20

/* ── Fragment offset field flags ───────────────────────────────────── */
#define IP_FLAG_DF      0x4000      /* Don't Fragment (network byte order bit) */

/* ── Protocol handler table ────────────────────────────────────────── */
#define IPV4_MAX_HANDLERS  8

/* ── IPv4 checksum helper ───────────────────────────────────────────── */
/* Compute Internet checksum over len bytes starting at data.
   Returns the checksum in network byte order. */
static inline u16 ip_checksum(const void *data, u16 len) {
    const u16 *p   = (const u16 *)data;
    u32        sum = 0;
    u16        n   = (u16)(len >> 1);

    while (n--) sum += *p++;

    if (len & 1)
        sum += *(const u8 *)p;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (u16)~sum;
}

#endif /* IPV4_INTERNAL_H */

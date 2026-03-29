/* udp_internal.h — private constants and on-wire structures for UDP */
#ifndef UDP_INTERNAL_H
#define UDP_INTERNAL_H

#include "../../lib/types.h"

/* ── UDP header (8 bytes) ───────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    u16 src_port;   /* source port, network byte order */
    u16 dst_port;   /* destination port, network byte order */
    u16 length;     /* header + data, network byte order */
    u16 checksum;   /* optional checksum (0 = not computed) */
} udp_hdr_t;

#define UDP_HDR_LEN         8

/* ── UDP pseudo-header for checksum calculation ─────────────────────── */
typedef struct __attribute__((packed)) {
    u32 src_ip;     /* network byte order */
    u32 dst_ip;     /* network byte order */
    u8  zero;       /* always 0 */
    u8  proto;      /* IPPROTO_UDP = 17 */
    u16 udp_len;    /* same as udp_hdr_t.length, network byte order */
} udp_pseudo_hdr_t;

/* ── Handler table size ─────────────────────────────────────────────── */
#define UDP_MAX_HANDLERS    16

#endif /* UDP_INTERNAL_H */

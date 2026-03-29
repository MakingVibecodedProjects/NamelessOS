/* icmp_internal.h — private constants and on-wire structures for ICMP */
#ifndef ICMP_INTERNAL_H
#define ICMP_INTERNAL_H

#include "../../lib/types.h"

/* ── ICMP type codes ────────────────────────────────────────────────── */
#define ICMP_TYPE_ECHO_REPLY    0
#define ICMP_TYPE_ECHO_REQUEST  8

/* ── ICMP header (8 bytes) ──────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    u8  type;
    u8  code;
    u16 checksum;
    u16 id;
    u16 seq;
} icmp_hdr_t;

#define ICMP_HDR_LEN    8

#endif /* ICMP_INTERNAL_H */

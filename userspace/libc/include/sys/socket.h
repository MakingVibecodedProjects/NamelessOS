#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <stdint.h>

/* ── Address families ─────────────────────────────────────────────── */
#define AF_INET     2

/* ── Socket types ─────────────────────────────────────────────────── */
#define SOCK_STREAM 1   /* TCP */
#define SOCK_DGRAM  2   /* UDP */

/* ── sockaddr_in ──────────────────────────────────────────────────── */
struct sockaddr_in {
    uint16_t sin_family;    /* AF_INET */
    uint16_t sin_port;      /* port in network byte order */
    uint32_t sin_addr;      /* IPv4 address in network byte order */
    uint8_t  sin_zero[8];   /* padding */
} __attribute__((packed));

/* ── Byte-order helpers ───────────────────────────────────────────── */
static inline uint16_t htons(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}
static inline uint16_t ntohs(uint16_t x) {
    return htons(x);
}
static inline uint32_t htonl(uint32_t x) {
    return ((x >> 24) & 0xff)       |
           (((x >> 16) & 0xff) << 8)  |
           (((x >> 8)  & 0xff) << 16) |
           ((x & 0xff) << 24);
}
static inline uint32_t ntohl(uint32_t x) {
    return htonl(x);
}

/* ── Prototypes ───────────────────────────────────────────────────── */
int socket  (int domain, int type, int protocol);
int bind    (int sockfd, const struct sockaddr_in *addr, uint32_t addrlen);
int listen  (int sockfd, int backlog);
int accept  (int sockfd, struct sockaddr_in *addr, uint32_t *addrlen);
int connect (int sockfd, const struct sockaddr_in *addr, uint32_t addrlen);
int send    (int sockfd, const void *buf, uint32_t len, int flags);
int recv    (int sockfd, void *buf, uint32_t len, int flags);
int shutdown(int sockfd, int how);

#endif /* _SYS_SOCKET_H */

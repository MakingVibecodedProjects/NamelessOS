#ifndef IPV4_H
#define IPV4_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* IP protocol numbers (also exported for ICMP/UDP/TCP) */
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

/* Handler called for each received IP datagram of a registered protocol.
   payload — bytes after the IP header.
   len     — payload length in bytes.
   src_ip  — sender IPv4 address (host byte order).
   dst_ip  — destination IPv4 address (host byte order). */
typedef void (*ipv4_rx_handler_t)(const u8 *payload, u16 len,
                                  u32 src_ip, u32 dst_ip);

/* Initialise IPv4: registers as the Ethernet IPv4 handler.
   Returns 0 on success. */
int  ipv4_init(void);

/* Register a handler for the given IP protocol number. */
int  ipv4_register(u8 proto, ipv4_rx_handler_t handler);

/* Set our IPv4 address and netmask (host byte order).
   Also updates the ARP layer so ARP replies work. */
void ipv4_set_addr(u32 ip, u32 netmask, u32 gateway);

/* Return our IPv4 address (host byte order), or 0 if not configured. */
u32  ipv4_get_addr(void);

/* Transmit one IPv4 datagram.
   dst_ip  — destination (host byte order); resolved via ARP.
   proto   — IP protocol number.
   payload — payload data.
   len     — payload length.
   Returns 0 on success, negative on failure. */
int  ipv4_send(u32 dst_ip, u8 proto, const u8 *payload, u16 len);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_ipv4;

#endif /* IPV4_H */

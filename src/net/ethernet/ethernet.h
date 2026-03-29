#ifndef ETHERNET_H
#define ETHERNET_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* EtherType constants (host byte order) */
#define ETHERTYPE_IPV4  0x0800
#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IPV6  0x86DD

/* Broadcast MAC address */
#define ETH_BROADCAST   { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

/* Handler called for each received frame of a registered ethertype.
   payload — pointer to the byte immediately after the Ethernet header.
   len     — payload length in bytes.
   src_mac — sender's MAC address (6 bytes).
   The handler must NOT free payload; it is freed by the ethernet layer. */
typedef void (*eth_rx_handler_t)(const u8 *payload, u16 len,
                                 const u8 src_mac[6]);

/* Initialise the Ethernet layer (registers itself as the e1000 RX callback).
   Returns 0 on success. */
int  ethernet_init(void);

/* Register a handler for frames with the given EtherType (host byte order).
   At most ETH_MAX_HANDLERS can be registered; returns 0 on success, -1 if full. */
int  ethernet_register(u16 ethertype, eth_rx_handler_t handler);

/* Build and transmit one Ethernet frame.
   dst     — destination MAC (6 bytes).
   type    — EtherType in host byte order.
   payload — payload data.
   len     — payload length (must be ≤ 1500).
   Returns 0 on success, negative on failure. */
int  ethernet_send(const u8 dst[6], u16 type,
                   const u8 *payload, u16 len);

/* Poll the NIC for received frames and dispatch to registered handlers.
   Should be called periodically (e.g. from a timer callback). */
void ethernet_poll(void);

/* Return our own MAC address. */
void ethernet_get_mac(u8 mac[6]);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_ethernet;

#endif /* ETHERNET_H */

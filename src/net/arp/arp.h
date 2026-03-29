#ifndef ARP_H
#define ARP_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* Initialise ARP: registers as the Ethernet ARP handler.
   Returns 0 on success. */
int  arp_init(void);

/* Look up ip (host byte order) in the ARP table.
   Copies the MAC into mac[6] and returns 0 on hit, -1 on miss. */
int  arp_lookup(u32 ip, u8 mac[6]);

/* Insert or update an ARP table entry (called on received ARP replies
   and gratuitous ARPs — also callable by upper layers). */
void arp_insert(u32 ip, const u8 mac[6]);

/* Send an ARP request for ip (host byte order). */
void arp_request(u32 ip);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_arp;

#endif /* ARP_H */

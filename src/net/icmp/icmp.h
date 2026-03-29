#ifndef ICMP_H
#define ICMP_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* Initialise ICMP: registers as the IPv4 ICMP handler.
   Returns 0 on success. */
int  icmp_init(void);

/* Send an ICMP echo request to dst_ip with given id and sequence number.
   data/len is optional payload (may be NULL/0).
   Returns 0 on success, negative on failure. */
int  icmp_ping(u32 dst_ip, u16 id, u16 seq, const u8 *data, u16 len);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_icmp;

#endif /* ICMP_H */

#ifndef DHCP_H
#define DHCP_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* Initialise the DHCP client: registers a UDP handler on port 68 and
   sends the first DHCPDISCOVER.  Returns 0 on success. */
int  dhcp_init(void);

/* Drive the DHCP state machine — call periodically (e.g. timer callback).
   Returns 0 while in progress, 1 when bound, -1 on failure. */
int  dhcp_tick(void);

/* Return 1 if a lease has been obtained, 0 otherwise. */
int  dhcp_bound(void);

/* Module descriptor — registered in module_registry after mod_socket. */
extern kernel_module_t mod_dhcp;

#endif /* DHCP_H */

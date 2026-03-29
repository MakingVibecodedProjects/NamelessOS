#ifndef SOCKET_H
#define SOCKET_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* Initialise the socket layer: installs syscall handlers for
   socket/bind/listen/connect/accept/sendto/recvfrom/shutdown.
   Returns 0 on success. */
int socket_init(void);

/* Module descriptor — registered in module_registry after mod_tcp. */
extern kernel_module_t mod_socket;

#endif /* SOCKET_H */

#ifndef UDP_H
#define UDP_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* Callback invoked for each received UDP datagram destined for a bound port.
   data    — UDP payload (after the header).
   len     — payload length in bytes.
   src_ip  — sender IPv4 address (host byte order).
   src_port — sender UDP port (host byte order).
   dst_port — destination UDP port (host byte order). */
typedef void (*udp_rx_handler_t)(const u8 *data, u16 len,
                                 u32 src_ip, u16 src_port, u16 dst_port);

/* Initialise UDP: registers as the IPv4 UDP protocol handler.
   Returns 0 on success. */
int  udp_init(void);

/* Register a handler for datagrams arriving on dst_port (host byte order).
   Pass dst_port=0 to receive all unmatched datagrams.
   Returns 0 on success, -1 if the table is full. */
int  udp_register(u16 dst_port, udp_rx_handler_t handler);

/* Send a UDP datagram.
   dst_ip   — destination IPv4 address (host byte order).
   src_port — source UDP port (host byte order).
   dst_port — destination UDP port (host byte order).
   data/len — payload.
   Returns 0 on success, negative on failure. */
int  udp_send(u32 dst_ip, u16 src_port, u16 dst_port,
              const u8 *data, u16 len);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_udp;

#endif /* UDP_H */

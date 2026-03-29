#ifndef E1000_H
#define E1000_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* Callback type invoked when a frame is received.
   buf  — pointer to the raw Ethernet frame (kernel heap allocation).
   len  — frame length in bytes.
   The callback MUST free buf with kfree() when it is done. */
typedef void (*e1000_rx_cb_t)(u8 *buf, u16 len);

/* Detect the e1000 NIC via PCI, set up MMIO, DMA rings, and link.
   Returns 0 on success, negative on failure (no NIC found, OOM, etc.). */
int   e1000_init(void);

/* Transmit one raw Ethernet frame.
   buf  — caller-allocated buffer containing the complete frame.
   len  — frame length in bytes (max 1514 for standard frames).
   Returns 0 on success, negative on failure. */
int   e1000_send(const u8 *buf, u16 len);

/* Register a callback that will be invoked for every received frame.
   Only one callback is supported; calling again replaces the previous one. */
void  e1000_register_rx_callback(e1000_rx_cb_t cb);

/* Return the device MAC address in mac[0..5]. */
void  e1000_get_mac(u8 mac[6]);

/* Poll the RX ring and deliver any received frames to the registered callback.
   Call this periodically (e.g. from the timer tick or a kernel thread). */
void  e1000_poll(void);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_e1000;

#endif /* E1000_H */

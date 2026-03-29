#ifndef TCP_H
#define TCP_H

#include "../../lib/types.h"
#include "../../lib/module.h"

/* Opaque connection handle — index into internal connection table.
   -1 means invalid / no connection. */
typedef int tcp_conn_id_t;

/* Callbacks supplied when connecting or listening.
   Both may be NULL.
   on_data  — called with received payload bytes (conn_id, data, len).
   on_close — called when the connection reaches CLOSED state. */
typedef void (*tcp_data_cb_t) (tcp_conn_id_t id, const u8 *data, u16 len);
typedef void (*tcp_close_cb_t)(tcp_conn_id_t id);

/* Initialise TCP: registers as the IPv4 TCP protocol handler.
   Returns 0 on success. */
int tcp_init(void);

/* Open a listening "socket" on local_port.
   Returns a conn_id on success (state=LISTEN), -1 on failure.
   on_data / on_close are inherited by accepted connections. */
tcp_conn_id_t tcp_listen(u16 local_port,
                         tcp_data_cb_t  on_data,
                         tcp_close_cb_t on_close);

/* Initiate an active connection to remote_ip:remote_port.
   Returns a conn_id (state=SYN_SENT), -1 on failure. */
tcp_conn_id_t tcp_connect(u32 remote_ip, u16 remote_port,
                          u16 local_port,
                          tcp_data_cb_t  on_data,
                          tcp_close_cb_t on_close);

/* Send data on an ESTABLISHED connection.
   Returns 0 on success, negative on failure. */
int tcp_send(tcp_conn_id_t id, const u8 *data, u16 len);

/* Initiate graceful close (send FIN).
   Returns 0 on success. */
int tcp_close(tcp_conn_id_t id);

/* Must be called periodically (e.g. from a timer callback) to drive
   retransmit timeouts and TIME_WAIT expiry. */
void tcp_tick(void);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_tcp;

#endif /* TCP_H */

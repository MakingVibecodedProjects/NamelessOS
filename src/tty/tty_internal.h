#ifndef TTY_INTERNAL_H
#define TTY_INTERNAL_H

/* ── Line buffer size ───────────────────────────────────────────────── */
#define TTY_LBUF_SIZE   256     /* cooked-mode line buffer              */

/* ── Read ring buffer ────────────────────────────────────────────────
   Completed lines are moved here; read() drains from this ring.        */
#define TTY_RBUF_SIZE   1024

#endif /* TTY_INTERNAL_H */

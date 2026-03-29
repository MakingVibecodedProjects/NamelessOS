#ifndef KEYBOARD_INTERNAL_H
#define KEYBOARD_INTERNAL_H

/* ── PS/2 ports ──────────────────────────────────────────────────── */
#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64

/* ── Scancode set 1 — make codes only (release = make | 0x80) ────── */
/* Index = make scancode (0x00–0x57), value = ASCII (0 = non-printable) */
#define KB_SC_MAX       0x58

/* ── Circular buffer ─────────────────────────────────────────────── */
/* Must be a power of two for the index mask trick. */
#define KB_BUF_SIZE     256
#define KB_BUF_MASK     (KB_BUF_SIZE - 1)

#endif /* KEYBOARD_INTERNAL_H */

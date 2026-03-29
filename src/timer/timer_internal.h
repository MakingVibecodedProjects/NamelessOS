#ifndef TIMER_INTERNAL_H
#define TIMER_INTERNAL_H

/* ── PIT (8253/8254) port map ────────────────────────────────────── */
#define PIT_CHANNEL0_DATA   0x40
#define PIT_COMMAND         0x43

/* Command byte: channel 0, lobyte/hibyte, rate generator mode 3 */
#define PIT_CMD_CHANNEL0    0x00   /* channel 0 */
#define PIT_CMD_ACCESS_LH   0x30   /* lo then hi byte */
#define PIT_CMD_MODE2       0x04   /* rate generator */
#define PIT_CMD_BINARY      0x00   /* 16-bit binary */
#define PIT_CMD             (PIT_CMD_CHANNEL0 | PIT_CMD_ACCESS_LH | \
                             PIT_CMD_MODE2    | PIT_CMD_BINARY)

/* PIT input frequency in Hz */
#define PIT_BASE_HZ         1193182UL

/* Target tick rate */
#define TIMER_HZ            1000U

/* Reload value = PIT_BASE_HZ / TIMER_HZ ≈ 1193 */
#define PIT_DIVISOR         ((u16)(PIT_BASE_HZ / TIMER_HZ))

/* Maximum number of registered tick callbacks */
#define TIMER_MAX_CALLBACKS 8

#endif /* TIMER_INTERNAL_H */

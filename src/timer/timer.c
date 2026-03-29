#include "timer.h"
#include "timer_internal.h"
#include "../serial/serial.h"
#include "../pic/pic.h"

/* ── Tick counter ────────────────────────────────────────────────── */
static volatile u64 ticks = 0;

/* ── Callback table ──────────────────────────────────────────────── */
static void (*callbacks[TIMER_MAX_CALLBACKS])(void);
static u8 callback_count = 0;

/* ── I/O helpers ─────────────────────────────────────────────────── */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ── IRQ0 handler ────────────────────────────────────────────────── */
static void timer_irq_handler(void) {
    ticks++;
    for (u8 i = 0; i < callback_count; i++)
        callbacks[i]();
}

/* ── Public API ──────────────────────────────────────────────────── */

u64 timer_get_ticks(void) {
    return ticks;
}

void ksleep(u32 ms) {
    u64 target = ticks + ms;
    while (ticks < target)
        __asm__ volatile ("pause");
}

int timer_register_callback(void (*fn)(void)) {
    if (callback_count >= TIMER_MAX_CALLBACKS) return -1;
    callbacks[callback_count++] = fn;
    return 0;
}

/* ── timer_dump ──────────────────────────────────────────────────── */
static void timer_dump(void) {
    klog(LOG_DEBUG, "[timer] ticks=%u", (unsigned int)ticks);
}

/* ── timer_init ──────────────────────────────────────────────────── */
int timer_init(void) {
    /* Program PIT channel 0: rate generator (mode 2), lobyte/hibyte */
    outb(PIT_COMMAND, PIT_CMD);
    outb(PIT_CHANNEL0_DATA, (u8)(PIT_DIVISOR & 0xFF));
    outb(PIT_CHANNEL0_DATA, (u8)(PIT_DIVISOR >> 8));

    /* Register handler and unmask IRQ0 */
    irq_register(0, timer_irq_handler);
    irq_enable(0);

    klog(LOG_INFO, "[timer] PIT channel 0 at %u Hz", TIMER_HZ);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_timer = {
    .name        = "timer",
    .initialized = false,
    .init        = timer_init,
    .dump        = timer_dump,
    .shutdown    = NULL,
};

#include "keyboard.h"
#include "keyboard_internal.h"
#include "../serial/serial.h"
#include "../pic/pic.h"

/* ── Scancode set 1 → ASCII table ───────────────────────────────── */
/* Index = make scancode; 0 = no ASCII representation */
static const u8 sc_to_ascii[KB_SC_MAX] = {
    /* 0x00 */ 0,
    /* 0x01 ESC    */ 0x1B,
    /* 0x02 1      */ '1',
    /* 0x03 2      */ '2',
    /* 0x04 3      */ '3',
    /* 0x05 4      */ '4',
    /* 0x06 5      */ '5',
    /* 0x07 6      */ '6',
    /* 0x08 7      */ '7',
    /* 0x09 8      */ '8',
    /* 0x0A 9      */ '9',
    /* 0x0B 0      */ '0',
    /* 0x0C -      */ '-',
    /* 0x0D =      */ '=',
    /* 0x0E BKSP   */ '\b',
    /* 0x0F TAB    */ '\t',
    /* 0x10 q      */ 'q',
    /* 0x11 w      */ 'w',
    /* 0x12 e      */ 'e',
    /* 0x13 r      */ 'r',
    /* 0x14 t      */ 't',
    /* 0x15 y      */ 'y',
    /* 0x16 u      */ 'u',
    /* 0x17 i      */ 'i',
    /* 0x18 o      */ 'o',
    /* 0x19 p      */ 'p',
    /* 0x1A [      */ '[',
    /* 0x1B ]      */ ']',
    /* 0x1C ENTER  */ '\n',
    /* 0x1D LCTRL  */ 0,
    /* 0x1E a      */ 'a',
    /* 0x1F s      */ 's',
    /* 0x20 d      */ 'd',
    /* 0x21 f      */ 'f',
    /* 0x22 g      */ 'g',
    /* 0x23 h      */ 'h',
    /* 0x24 j      */ 'j',
    /* 0x25 k      */ 'k',
    /* 0x26 l      */ 'l',
    /* 0x27 ;      */ ';',
    /* 0x28 '      */ '\'',
    /* 0x29 `      */ '`',
    /* 0x2A LSHIFT */ 0,
    /* 0x2B \      */ '\\',
    /* 0x2C z      */ 'z',
    /* 0x2D x      */ 'x',
    /* 0x2E c      */ 'c',
    /* 0x2F v      */ 'v',
    /* 0x30 b      */ 'b',
    /* 0x31 n      */ 'n',
    /* 0x32 m      */ 'm',
    /* 0x33 ,      */ ',',
    /* 0x34 .      */ '.',
    /* 0x35 /      */ '/',
    /* 0x36 RSHIFT */ 0,
    /* 0x37 * (KP) */ '*',
    /* 0x38 LALT   */ 0,
    /* 0x39 SPACE  */ ' ',
    /* 0x3A CAPS   */ 0,
    /* 0x3B F1     */ 0,
    /* 0x3C F2     */ 0,
    /* 0x3D F3     */ 0,
    /* 0x3E F4     */ 0,
    /* 0x3F F5     */ 0,
    /* 0x40 F6     */ 0,
    /* 0x41 F7     */ 0,
    /* 0x42 F8     */ 0,
    /* 0x43 F9     */ 0,
    /* 0x44 F10    */ 0,
    /* 0x45 NUMLK  */ 0,
    /* 0x46 SCRLK  */ 0,
    /* 0x47 7 (KP) */ '7',
    /* 0x48 8 (KP) */ '8',
    /* 0x49 9 (KP) */ '9',
    /* 0x4A - (KP) */ '-',
    /* 0x4B 4 (KP) */ '4',
    /* 0x4C 5 (KP) */ '5',
    /* 0x4D 6 (KP) */ '6',
    /* 0x4E + (KP) */ '+',
    /* 0x4F 1 (KP) */ '1',
    /* 0x50 2 (KP) */ '2',
    /* 0x51 3 (KP) */ '3',
    /* 0x52 0 (KP) */ '0',
    /* 0x53 . (KP) */ '.',
    /* 0x54        */ 0,
    /* 0x55        */ 0,
    /* 0x56        */ 0,
    /* 0x57 F11    */ 0,
};

/* ── Circular buffer ─────────────────────────────────────────────── */
static volatile u8  kb_buf[KB_BUF_SIZE];
static volatile u32 kb_head = 0;   /* write index */
static volatile u32 kb_tail = 0;   /* read index  */

static inline void buf_push(u8 ch) {
    u32 next = (kb_head + 1) & KB_BUF_MASK;
    if (next == kb_tail) return;   /* buffer full — drop */
    kb_buf[kb_head] = ch;
    kb_head = next;
}

/* ── I/O helper ──────────────────────────────────────────────────── */
static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* ── IRQ1 handler ────────────────────────────────────────────────── */
static void keyboard_irq_handler(void) {
    u8 sc = inb(KB_DATA_PORT);

    /* Ignore key-release events (bit 7 set) and extended prefix 0xE0 */
    if (sc & 0x80) return;
    if (sc == 0xE0) return;

    if (sc < KB_SC_MAX) {
        u8 ch = sc_to_ascii[sc];
        if (ch) buf_push(ch);
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

i32 keyboard_getc(void) {
    if (kb_tail == kb_head) return -1;
    u8 ch  = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) & KB_BUF_MASK;
    return (i32)ch;
}

/* ── keyboard_dump ───────────────────────────────────────────────── */
static void keyboard_dump(void) {
    u32 used = (kb_head - kb_tail) & KB_BUF_MASK;
    klog(LOG_DEBUG, "[keyboard] buf used=%u/%u", used, (u32)KB_BUF_SIZE);
}

/* ── keyboard_init ───────────────────────────────────────────────── */
int keyboard_init(void) {
    /* Flush any stale byte in the output buffer */
    if (inb(KB_STATUS_PORT) & 0x01)
        (void)inb(KB_DATA_PORT);

    irq_register(1, keyboard_irq_handler);
    irq_enable(1);

    klog(LOG_INFO, "[keyboard] PS/2 keyboard ready");
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_keyboard = {
    .name        = "keyboard",
    .initialized = false,
    .init        = keyboard_init,
    .dump        = keyboard_dump,
    .shutdown    = NULL,
};

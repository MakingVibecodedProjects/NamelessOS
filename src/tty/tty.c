#include "tty.h"
#include "tty_internal.h"
#include "../devfs/devfs.h"
#include "../vfs/vfs.h"
#include "../vga/vga.h"
#include "../keyboard/keyboard.h"
#include "../timer/timer.h"
#include "../serial/serial.h"
#include "../lib/string.h"
#include "../lib/types.h"

/* ── Cooked-mode line buffer ────────────────────────────────────────── */
/* Accumulates raw keystrokes; flushes to read ring on '\n'.            */
static char lbuf[TTY_LBUF_SIZE];
static u32  lbuf_len = 0;

/* ── Read ring buffer ────────────────────────────────────────────────
   Completed lines land here; tty_read() drains from this ring.         */
static u8  rbuf[TTY_RBUF_SIZE];
static u32 rbuf_head = 0;   /* next byte to consume */
static u32 rbuf_tail = 0;   /* next free slot       */

static u32 rbuf_avail(void) {
    return (rbuf_tail - rbuf_head) % TTY_RBUF_SIZE;
}

static void rbuf_push(u8 c) {
    u32 next = (rbuf_tail + 1) % TTY_RBUF_SIZE;
    if (next == rbuf_head) return;   /* full — drop */
    rbuf[rbuf_tail] = c;
    rbuf_tail = next;
}

static i32 rbuf_pop(void) {
    if (rbuf_head == rbuf_tail) return -1;
    u8 c = rbuf[rbuf_head];
    rbuf_head = (rbuf_head + 1) % TTY_RBUF_SIZE;
    return (i32)c;
}

/* ── tty_poll — called on every timer tick to drain keyboard ─────────
   Implements minimal cooked (canonical) mode:
     - printable chars → echo to VGA + accumulate in lbuf
     - backspace       → erase last char from lbuf + VGA
     - '\r' or '\n'    → append '\n' to lbuf, flush to rbuf, echo '\n' */
void tty_poll(void) {
    i32 c;
    while ((c = keyboard_getc()) != -1) {
        char ch = (char)(u8)c;

        if (ch == '\r' || ch == '\n') {
            /* End of line — flush lbuf → rbuf including the newline */
            for (u32 i = 0; i < lbuf_len; i++)
                rbuf_push((u8)lbuf[i]);
            rbuf_push((u8)'\n');
            lbuf_len = 0;
            vga_putchar('\n');
        } else if (ch == '\b' || ch == 127) {
            /* Backspace — erase last character */
            if (lbuf_len > 0) {
                lbuf_len--;
                /* Overwrite on VGA: BS + space + BS */
                vga_putchar('\b');
                vga_putchar(' ');
                vga_putchar('\b');
            }
        } else if (ch >= 0x20 && ch < 0x7F) {
            /* Printable: echo and buffer */
            if (lbuf_len < TTY_LBUF_SIZE - 1) {
                lbuf[lbuf_len++] = ch;
                vga_putchar(ch);
            }
        }
        /* Control chars other than BS/CR/LF are silently dropped */
    }
}

/* ── VFS ops for /dev/tty0 ──────────────────────────────────────────── */

static i32 tty_read(vfs_node_t *node, u32 offset, u32 size, u8 *buf) {
    (void)node; (void)offset;
    if (!buf || size == 0) return 0;

    u32 n = 0;
    while (n < size) {
        /* Drain any already-buffered data */
        if (rbuf_avail() > 0) {
            i32 ch = rbuf_pop();
            if (ch < 0) break;
            buf[n++] = (u8)ch;
            /* Stop at newline — matches POSIX line-buffered read */
            if ((char)ch == '\n') break;
            continue;
        }
        /* Poll keyboard until we have at least one byte */
        tty_poll();
        if (rbuf_avail() == 0)
            break;   /* still nothing — return what we have (non-blocking) */
    }
    return (i32)n;
}

static i32 tty_write(vfs_node_t *node, u32 offset, u32 size, const u8 *buf) {
    (void)node; (void)offset;
    if (!buf) return 0;
    for (u32 i = 0; i < size; i++)
        vga_putchar((char)buf[i]);
    return (i32)size;
}

static fs_ops_t tty_ops = {
    .read    = tty_read,
    .write   = tty_write,
    .open    = NULL,
    .close   = NULL,
    .readdir = NULL,
    .finddir = NULL,
};

/* ── Module init / dump ─────────────────────────────────────────────── */
static int tty_init_impl(void) {
    lbuf_len  = 0;
    rbuf_head = 0;
    rbuf_tail = 0;

    /* Poll keyboard on every timer tick to fill the read ring */
    timer_register_callback(tty_poll);

    /* Register /dev/tty0 */
    if (devfs_register("tty0", VFS_CHARDEV, &tty_ops) < 0) {
        klog(LOG_ERROR, "[tty] failed to register /dev/tty0\n");
        return -1;
    }

    klog(LOG_INFO, "[tty] /dev/tty0 ready\n");
    return 0;
}

static void tty_dump(void) {
    klog(LOG_INFO, "[tty] lbuf_len=%u rbuf_avail=%u\n",
         (unsigned)lbuf_len, (unsigned)rbuf_avail());
}

/* ── Module descriptor ──────────────────────────────────────────────── */
kernel_module_t mod_tty = {
    .name        = "tty",
    .init        = tty_init_impl,
    .dump        = tty_dump,
    .initialized = false,
};

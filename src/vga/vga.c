#include "vga.h"
#include "../lib/string.h"
#include "../serial/serial.h"
#include "../vmm/vmm_internal.h"   /* PHYS_TO_VIRT — access via higher-half window */

/* ── VGA text mode constants ─────────────────────────────────────── */
#define VGA_COLS         80
#define VGA_ROWS         25
#define VGA_BUF_PHYS     0xB8000ULL
#define VGA_DEFAULT_ATTR 0x07   /* White on black */

/* Accessed via higher-half window so it works regardless of which
   user PML4 is loaded (PML4[511] is always shared from the boot PML4). */
static volatile u16 *const vga_buf = (volatile u16 *)PHYS_TO_VIRT(VGA_BUF_PHYS);

static int vga_col = 0;
static int vga_row = 0;

/* ── I/O port helpers (cursor) ───────────────────────────────────── */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

#define VGA_CTRL_REG  0x3D4
#define VGA_DATA_REG  0x3D5
#define VGA_CURSOR_HI 0x0E
#define VGA_CURSOR_LO 0x0F

static void vga_update_cursor(void) {
    u16 pos = (u16)(vga_row * VGA_COLS + vga_col);
    outb(VGA_CTRL_REG, VGA_CURSOR_HI);
    outb(VGA_DATA_REG, (u8)(pos >> 8));
    outb(VGA_CTRL_REG, VGA_CURSOR_LO);
    outb(VGA_DATA_REG, (u8)(pos & 0xFF));
}

/* ── vga_clear ───────────────────────────────────────────────────── */
void vga_clear(void) {
    u16 blank = (u16)((VGA_DEFAULT_ATTR << 8) | ' ');
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga_buf[i] = blank;
    vga_col = 0;
    vga_row = 0;
    vga_update_cursor();
}

/* ── scroll ──────────────────────────────────────────────────────── */
static void vga_scroll(void) {
    for (int r = 1; r < VGA_ROWS; r++)
        for (int c = 0; c < VGA_COLS; c++)
            vga_buf[(r - 1) * VGA_COLS + c] = vga_buf[r * VGA_COLS + c];
    u16 blank = (u16)((VGA_DEFAULT_ATTR << 8) | ' ');
    for (int c = 0; c < VGA_COLS; c++)
        vga_buf[(VGA_ROWS - 1) * VGA_COLS + c] = blank;
    vga_row = VGA_ROWS - 1;
}

/* ── vga_putchar ─────────────────────────────────────────────────── */
void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
        if (vga_col >= VGA_COLS) { vga_col = 0; vga_row++; }
    } else {
        vga_buf[vga_row * VGA_COLS + vga_col] =
            (u16)((VGA_DEFAULT_ATTR << 8) | (unsigned char)c);
        vga_col++;
        if (vga_col >= VGA_COLS) { vga_col = 0; vga_row++; }
    }
    if (vga_row >= VGA_ROWS) vga_scroll();
    vga_update_cursor();
}

/* ── vga_write ───────────────────────────────────────────────────── */
void vga_write(const char *s) {
    while (*s) vga_putchar(*s++);
}

/* ── vga_dump ────────────────────────────────────────────────────── */
void vga_dump(void) {
    klog(LOG_DEBUG, "[vga] 80x25 text mode, cursor at (%d, %d)", vga_col, vga_row);
}

/* ── vga_init ────────────────────────────────────────────────────── */
int vga_init(void) {
    vga_clear();
    klog(LOG_INFO, "[vga] 80x25 text mode ready");
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_vga = {
    .name        = "vga",
    .initialized = false,
    .init        = vga_init,
    .dump        = vga_dump,
    .shutdown    = NULL,
};

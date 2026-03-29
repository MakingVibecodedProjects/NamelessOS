#ifndef VGA_H
#define VGA_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Initialize 80×25 VGA text mode, clear the screen. Returns 0 on success. */
int  vga_init(void);

/* Write a single character at the current cursor position. */
void vga_putchar(char c);

/* Write a NUL-terminated string to VGA. */
void vga_write(const char *s);

/* Clear the screen and reset cursor to (0, 0). */
void vga_clear(void);

/* Dump VGA module state (used by module_registry). */
void vga_dump(void);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_vga;

#endif /* VGA_H */

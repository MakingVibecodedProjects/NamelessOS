#ifndef PRINTF_H
#define PRINTF_H

#include "types.h"

/* Function pointer type for the output backend (VGA or serial). */
typedef void (*kprintf_putchar_fn)(char c);

/* Register the output function used by kprintf/vkprintf. */
void kprintf_set_output(kprintf_putchar_fn fn);

/* Freestanding kernel printf — supports %s %d %u %x %X %p %c %% */
void kprintf(const char *fmt, ...);

/* va_list variant — caller supplies the va_list directly. */
typedef __builtin_va_list kprintf_va_list;
void vkprintf(const char *fmt, kprintf_va_list ap);

#endif /* PRINTF_H */

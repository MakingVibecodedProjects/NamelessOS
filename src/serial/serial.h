#ifndef SERIAL_H
#define SERIAL_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Log levels */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_PANIC
} log_level_t;

/* Initialize COM1 at 115200 baud. Returns 0 on success. */
int  serial_init(void);

/* Write a single character to COM1. */
void serial_putchar(char c);

/* Write a NUL-terminated string to COM1. */
void serial_write(const char *s);

/* Dump serial module state (used by module_registry). */
void serial_dump(void);

/* Structured kernel log: [LEVEL] message, with printf-style formatting. */
void klog(log_level_t level, const char *fmt, ...);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_serial;

#endif /* SERIAL_H */

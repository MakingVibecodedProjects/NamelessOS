#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>

/* ── vsnprintf ───────────────────────────────────────────────────── */
/* Supports: %c %s %d %i %u %x %X %p %% (no width/precision/length) */

static void buf_append(char *buf, size_t *pos, size_t max, char c) {
    if (*pos + 1 < max) buf[(*pos)++] = c;
}

static void buf_str(char *buf, size_t *pos, size_t max, const char *s) {
    if (!s) s = "(null)";
    while (*s) buf_append(buf, pos, max, *s++);
}

static void buf_uint(char *buf, size_t *pos, size_t max,
                     uint64_t val, int base, int upper) {
    char tmp[32];
    int  i = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (val == 0) { buf_append(buf, pos, max, '0'); return; }
    while (val) { tmp[i++] = digits[val % (uint64_t)base]; val /= (uint64_t)base; }
    while (i--) buf_append(buf, pos, max, tmp[i]);
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    size_t pos = 0;
    if (!n) return 0;

    while (*fmt) {
        if (*fmt != '%') { buf_append(buf, &pos, n, *fmt++); continue; }
        fmt++;  /* skip '%' */
        switch (*fmt++) {
        case 'c':
            buf_append(buf, &pos, n, (char)va_arg(ap, int));
            break;
        case 's':
            buf_str(buf, &pos, n, va_arg(ap, const char *));
            break;
        case 'd': case 'i': {
            int64_t v = (int64_t)va_arg(ap, int);
            if (v < 0) { buf_append(buf, &pos, n, '-'); v = -v; }
            buf_uint(buf, &pos, n, (uint64_t)v, 10, 0);
            break;
        }
        case 'u':
            buf_uint(buf, &pos, n, (uint64_t)va_arg(ap, unsigned int), 10, 0);
            break;
        case 'x':
            buf_uint(buf, &pos, n, (uint64_t)va_arg(ap, unsigned int), 16, 0);
            break;
        case 'X':
            buf_uint(buf, &pos, n, (uint64_t)va_arg(ap, unsigned int), 16, 1);
            break;
        case 'p':
            buf_append(buf, &pos, n, '0');
            buf_append(buf, &pos, n, 'x');
            buf_uint(buf, &pos, n, (uint64_t)(uintptr_t)va_arg(ap, void *), 16, 0);
            break;
        case '%':
            buf_append(buf, &pos, n, '%');
            break;
        default:
            buf_append(buf, &pos, n, '?');
            break;
        }
    }
    buf[pos] = '\0';
    return (int)pos;
}

int vsprintf(char *buf, const char *fmt, va_list ap) {
    return vsnprintf(buf, (size_t)-1, fmt, ap);
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

/* ── printf / puts / putchar — write to stdout (fd 1) ───────────── */

int vprintf(const char *fmt, va_list ap) {
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0) write(STDOUT_FILENO, buf, (size_t)n);
    return n;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

int putchar(int c) {
    char ch = (char)c;
    write(STDOUT_FILENO, &ch, 1);
    return c;
}

int puts(const char *s) {
    size_t len = strlen(s);
    write(STDOUT_FILENO, s, len);
    write(STDOUT_FILENO, "\n", 1);
    return (int)len + 1;
}

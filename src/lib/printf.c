#include "printf.h"
#include "string.h"

/* ── va_list shim ────────────────────────────────────────────────── */
typedef __builtin_va_list va_list;
#define va_start(v, l)  __builtin_va_start(v, l)
#define va_arg(v, t)    __builtin_va_arg(v, t)
#define va_end(v)       __builtin_va_end(v)
#define va_copy(d, s)   __builtin_va_copy(d, s)

/* ── output backend ─────────────────────────────────────────────── */
static kprintf_putchar_fn kprintf_output_fn = NULL;

void kprintf_set_output(kprintf_putchar_fn fn) {
    kprintf_output_fn = fn;
}

static void emit(char c) {
    if (kprintf_output_fn) kprintf_output_fn(c);
}

static void emit_str(const char *s) {
    if (!s) { emit('('); emit('n'); emit('u'); emit('l'); emit(')'); return; }
    while (*s) emit(*s++);
}

static void emit_uint(u64 val, int base, bool upper) {
    static const char digits_lo[] = "0123456789abcdef";
    static const char digits_hi[] = "0123456789ABCDEF";
    const char *digits = upper ? digits_hi : digits_lo;
    char buf[64];
    int  i = 0;
    if (val == 0) { emit('0'); return; }
    while (val) {
        buf[i++] = digits[val % (u64)base];
        val /= (u64)base;
    }
    while (i--) emit(buf[i]);
}

static void emit_int(i64 val) {
    if (val < 0) { emit('-'); emit_uint((u64)(-val), 10, false); return; }
    emit_uint((u64)val, 10, false);
}

/* ── vkprintf (shared implementation) ──────────────────────────── */
void vkprintf(const char *fmt, kprintf_va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') { emit(*fmt); continue; }
        fmt++;
        switch (*fmt) {
            case 's': emit_str(va_arg(ap, const char *));                  break;
            case 'd': emit_int((i64)va_arg(ap, int));                      break;
            case 'u': emit_uint((u64)va_arg(ap, unsigned int), 10, false); break;
            case 'x': emit_uint((u64)va_arg(ap, unsigned int), 16, false); break;
            case 'X': emit_uint((u64)va_arg(ap, unsigned int), 16, true);  break;
            case 'p': emit('0'); emit('x');
                      emit_uint((u64)(usize)va_arg(ap, void *), 16, false); break;
            case 'c': emit((char)va_arg(ap, int));                         break;
            case '%': emit('%');                                            break;
            default:  emit('%'); emit(*fmt);                               break;
        }
    }
}

/* ── kprintf ────────────────────────────────────────────────────── */
void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vkprintf(fmt, ap);
    va_end(ap);
}

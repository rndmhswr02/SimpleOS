#include "kprintf.h"
#include "console.h"
#include <stdarg.h>
#include <stdint.h>

/* Print an unsigned number in the given base, with optional padding. */
static void put_number(uint32_t value, uint32_t base, int negative,
                       int width, char pad) {
    char buf[32];
    int i = 0;

    if (value == 0)
        buf[i++] = '0';
    while (value > 0) {
        buf[i++] = "0123456789abcdef"[value % base];
        value /= base;
    }

    int len = i + (negative ? 1 : 0);

    if (pad == ' ') {                       /* pad with spaces before the sign */
        while (len < width) { kputc(' '); len++; }
    }
    if (negative)
        kputc('-');
    if (pad == '0') {                       /* pad with zeros after the sign */
        while (len < width) { kputc('0'); len++; }
    }
    while (i > 0)
        kputc(buf[--i]);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            kputc(*fmt);
            continue;
        }

        fmt++;                              /* skip '%' */

        char pad = ' ';
        int width = 0;
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
        case 'd': {
            int v = va_arg(ap, int);
            if (v < 0)
                put_number(0u - (uint32_t)v, 10, 1, width, pad);
            else
                put_number((uint32_t)v, 10, 0, width, pad);
            break;
        }
        case 'u':
            put_number(va_arg(ap, uint32_t), 10, 0, width, pad);
            break;
        case 'x':
            put_number(va_arg(ap, uint32_t), 16, 0, width, pad);
            break;
        case 'p':
            kputc('0');
            kputc('x');
            put_number((uint32_t)(uintptr_t)va_arg(ap, void *), 16, 0, 8, '0');
            break;
        case 'c':
            kputc((char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s) kputc(*s++);
            break;
        }
        case '%':
            kputc('%');
            break;
        case '\0':                          /* format ended after a lone '%' */
            va_end(ap);
            return;
        default:                            /* unknown specifier: print it as-is */
            kputc('%');
            kputc(*fmt);
            break;
        }
    }

    va_end(ap);
}
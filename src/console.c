#include "console.h"
#include "serial.h"
#include "vga.h"

void kputc(char c) {
    term_putc(c);
    if (c == '\n')
        serial_putc('\r');     /* serial terminals want CR+LF */
    serial_putc(c);
}

void kprint(const char *s) {
    while (*s)
        kputc(*s++);
}
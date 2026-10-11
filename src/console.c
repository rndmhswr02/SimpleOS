#include "console.h"
#include "cpu.h"
#include "serial.h"
#include "vga.h"

void kputc(char c) {
    term_putc(c);
    if (c == '\n')
        serial_putc('\r');     /* serial terminals want CR+LF */
    serial_putc(c);
}

void kprint(const char *s) {
    uint32_t flags = irq_save();
    while (*s)
        kputc(*s++);
    irq_restore(flags);
}
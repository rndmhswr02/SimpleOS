#include "console.h"
#include "serial.h"
#include "vga.h"

void kprint(const char *s) {
    term_print(s);
    serial_print(s);
}
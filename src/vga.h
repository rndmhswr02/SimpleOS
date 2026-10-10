#ifndef VGA_H
#define VGA_H

#include <stdint.h>

void term_clear(void);
void term_putc(char c);
void term_print(const char *s);
void term_set_color(uint8_t color);   /* high nibble = background, low = foreground */

#endif
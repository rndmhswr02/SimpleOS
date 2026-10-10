#include "vga.h"
#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;
static size_t term_row = 0;
static size_t term_col = 0;
static uint8_t term_color = 0x0F;   /* white on black */

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void term_clear(void) {
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA[i] = vga_entry(' ', term_color);
    term_row = 0;
    term_col = 0;
}

static void term_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA[(y - 1) * VGA_WIDTH + x] = VGA[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
    term_row = VGA_HEIGHT - 1;
}

void term_putc(char c) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            VGA[term_row * VGA_WIDTH + term_col] = vga_entry(' ', term_color);
        }
    } else {
        VGA[term_row * VGA_WIDTH + term_col] = vga_entry(c, term_color);
        if (++term_col == VGA_WIDTH) {
            term_col = 0;
            term_row++;
        }
    }
    if (term_row >= VGA_HEIGHT)
        term_scroll();
}

void term_print(const char *s) {
    while (*s) term_putc(*s++);
}

void term_set_color(uint8_t color) {
    term_color = color;
}
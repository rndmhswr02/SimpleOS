#include <stdint.h>
#include <stddef.h>

/* ---------- Port I/O ---------- */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------- Serial (COM1): output appears in your terminal with -serial stdio ---------- */

#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00);    /* disable interrupts */
    outb(COM1 + 3, 0x80);    /* enable DLAB (set baud divisor) */
    outb(COM1 + 0, 0x03);    /* divisor low byte: 38400 baud */
    outb(COM1 + 1, 0x00);    /* divisor high byte */
    outb(COM1 + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7);    /* enable FIFO */
    outb(COM1 + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

static void serial_putc(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0) { }   /* wait for transmit buffer empty */
    outb(COM1, (uint8_t)c);
}

static void serial_print(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

/* ---------- VGA text mode (80x25) at 0xB8000 ---------- */

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;
static size_t term_row = 0;
static size_t term_col = 0;
static uint8_t term_color = 0x0F;   /* white on black */

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void term_clear(void) {
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

static void term_putc(char c) {
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

static void term_print(const char *s) {
    while (*s) term_putc(*s++);
}

/* Print to both the screen and the serial port */
static void kprint(const char *s) {
    term_print(s);
    serial_print(s);
}

/* ---------- Keyboard (PS/2, polling, scancode set 1) ---------- */

static const char scancode_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t', 'q','w','e','r','t','y','u','i','o','p','[',']', '\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

/* Blocks until a key is pressed, returns its ASCII character (no shift support yet) */
static char keyboard_getchar(void) {
    for (;;) {
        if (inb(0x64) & 0x01) {                 /* output buffer full? */
            uint8_t sc = inb(0x60);
            if (sc & 0x80) continue;            /* key release: ignore */
            if (sc < 128 && scancode_map[sc])
                return scancode_map[sc];
        }
    }
}

/* ---------- Tiny string helpers (we have no libc!) ---------- */

static int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int kstarts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

/* ---------- Shell ---------- */

#define LINE_MAX 128

static void shell_run(void) {
    char line[LINE_MAX];

    for (;;) {
        kprint("\nmyos> ");

        size_t len = 0;
        for (;;) {
            char c = keyboard_getchar();

            if (c == '\n') {
                term_putc('\n');
                break;
            } else if (c == '\b') {
                if (len > 0) {
                    len--;
                    term_putc('\b');
                }
            } else if (len < LINE_MAX - 1) {
                line[len++] = c;
                term_putc(c);
            }
        }
        line[len] = '\0';

        if (len == 0) {
            continue;
        } else if (kstrcmp(line, "help") == 0) {
            kprint("Commands: help, clear, about, echo <text>, halt\n");
        } else if (kstrcmp(line, "clear") == 0) {
            term_clear();
        } else if (kstrcmp(line, "about") == 0) {
            kprint("MyOS v0.1: a tiny x86 kernel written from scratch.\n");
        } else if (kstarts_with(line, "echo ")) {
            kprint(line + 5);
            kprint("\n");
        } else if (kstrcmp(line, "halt") == 0) {
            kprint("Halting CPU. Close the window to exit.\n");
            for (;;) __asm__ volatile ("cli; hlt");
        } else {
            kprint("Unknown command. Type 'help'.\n");
        }
    }
}

/* ---------- Kernel entry ---------- */

void kernel_main(uint32_t magic, uint32_t mbi_addr) {
    (void)mbi_addr;

    serial_init();
    term_clear();

    kprint("Hello from my kernel!\n");

    if (magic == 0x2BADB002)
        kprint("Booted via Multiboot. All good.\n");
    else
        kprint("Warning: not booted via Multiboot.\n");

    kprint("\nThis is the beginning of your OS.\n");

    shell_run();
}
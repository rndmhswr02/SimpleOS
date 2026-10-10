#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "kprintf.h"
#include "kstring.h"
#include "timer.h"
#include "vga.h"
#include <stddef.h>
#include <stdint.h>

#define SHELL_LINE_MAX 128

/* ---- Deliberate crashes (Phase 1) ---- */

static void test_div0(void) {
    volatile int a = 1;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}

static void test_gpf(void) {
    __asm__ volatile ("mov $0x28, %%ax\n\tmov %%ax, %%ds" : : : "ax");
}

static void test_ud(void) {
    __asm__ volatile ("ud2");
}

static void test_int3(void) {
    __asm__ volatile ("int3");
}

static void run_test(const char *name) {
    if (kstrcmp(name, "div0") == 0)       test_div0();
    else if (kstrcmp(name, "gpf") == 0)   test_gpf();
    else if (kstrcmp(name, "ud") == 0)    test_ud();
    else if (kstrcmp(name, "int3") == 0)  test_int3();
    else kprintf("Unknown test '%s'. Try: div0, gpf, ud, int3\n", name);
}

/* ---- Timer commands (Phase 2) ---- */

static void cmd_uptime(void) {
    uint32_t t  = timer_ticks();
    uint32_t hz = timer_hz();

    /* Split into seconds and milliseconds to avoid 32-bit overflow in t * 1000 */
    uint32_t secs = t / hz;
    uint32_t ms   = (t % hz) * 1000 / hz;

    kprintf("Up %u.%03u s (%u ticks at %u Hz)\n", secs, ms, t, hz);
}

static void cmd_sleep(const char *arg) {
    uint32_t ms;
    if (!kparse_uint(arg, &ms)) {
        kprintf("Usage: sleep <milliseconds>\n");
        return;
    }
    kprintf("Sleeping %u ms...\n", ms);
    timer_sleep_ms(ms);
    kprintf("Done.\n");
}

/* ---- Shell ---- */

void shell_run(void) {
    char line[SHELL_LINE_MAX];

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
            } else if (len < SHELL_LINE_MAX - 1) {
                line[len++] = c;
                term_putc(c);
            }
        }
        line[len] = '\0';

        if (len == 0) {
            continue;
        } else if (kstrcmp(line, "help") == 0) {
            kprintf("Commands: help, clear, about, echo <text>, uptime,\n");
            kprintf("          sleep <ms>, test <name>, halt\n");
            kprintf("Tests:    div0, gpf, ud, int3\n");
        } else if (kstrcmp(line, "clear") == 0) {
            term_clear();
        } else if (kstrcmp(line, "about") == 0) {
            kprintf("MyOS v%d.%d: GDT, IDT, PIC, PIT timer, IRQ keyboard.\n", 0, 3);
        } else if (kstarts_with(line, "echo ")) {
            kprintf("%s\n", line + 5);
        } else if (kstrcmp(line, "uptime") == 0) {
            cmd_uptime();
        } else if (kstarts_with(line, "sleep ")) {
            cmd_sleep(line + 6);
        } else if (kstarts_with(line, "test ")) {
            run_test(line + 5);
        } else if (kstrcmp(line, "halt") == 0) {
            kprintf("Halting CPU. Close the window to exit.\n");
            for (;;) __asm__ volatile ("cli; hlt");
        } else {
            kprintf("Unknown command. Type 'help'.\n");
        }
    }
}
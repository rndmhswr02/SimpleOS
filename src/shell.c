#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "kstring.h"
#include "vga.h"
#include <stddef.h>

#define SHELL_LINE_MAX 128

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
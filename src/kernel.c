#include <stdint.h>
#include "console.h"
#include "serial.h"
#include "shell.h"
#include "vga.h"
#include "kprintf.h"

#define MULTIBOOT_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t mbi_addr) {
    (void)mbi_addr;

    serial_init();
    term_clear();

    kprint("Hello from my kernel!\n");

    if (magic == MULTIBOOT_MAGIC)
        kprint("Booted via Multiboot. All good.\n");
    else
        kprint("Warning: not booted via Multiboot.\n");

    kprint("\nThis is the beginning of your OS.\n");
    kprint("Type 'help' for commands.\n");

    shell_run();
}
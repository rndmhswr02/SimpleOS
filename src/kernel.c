#include <stdint.h>
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "kprintf.h"
#include "serial.h"
#include "shell.h"
#include "timer.h"
#include "vga.h"

#define MULTIBOOT_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t mbi_addr) {
    serial_init();
    term_clear();

    kprintf("Hello from my kernel!\n");

    if (magic == MULTIBOOT_MAGIC)
        kprintf("Multiboot magic %08x: OK\n", magic);
    else
        kprintf("Warning: bad Multiboot magic %08x\n", magic);
    kprintf("Multiboot info at %p\n", (void *)(uintptr_t)mbi_addr);

    gdt_init();
    kprintf("[ok] GDT loaded\n");

    idt_init();
    kprintf("[ok] IDT loaded (CPU exceptions 0-31)\n");

    irq_init();
    kprintf("[ok] PIC remapped (IRQs at vectors 32-47, all masked)\n");
    
    timer_init(100);       /* 100 ticks per second */
    kprintf("[ok] PIT timer at %u Hz\n", timer_hz());

    __asm__ volatile ("sti");   /* from here on, hardware interrupts are live */
    kprintf("[ok] Interrupts enabled\n");

    kprintf("\nType 'help' for commands.\n");

    shell_run();
}
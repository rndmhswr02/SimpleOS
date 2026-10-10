#include <stdint.h>
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "keyboard.h"
#include "kheap.h"
#include "kprintf.h"
#include "paging.h"
#include "pmm.h"
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
    kprintf("[ok] IDT loaded (exceptions 0-31, IRQs 32-47)\n");

    /* Memory management comes right after the IDT (so page faults are reported)
     * and before interrupts are enabled (so nothing interrupts the paging switch). */
    pmm_init(mbi_addr);
    kprintf("[ok] PMM: %u MiB RAM, %u KiB free\n",
            pmm_ram_frames() >> 8, pmm_free_frames() << 2);

    uint32_t mapped_mib = paging_init();
    kprintf("[ok] Paging on: %u MiB identity-mapped, page 0 unmapped\n", mapped_mib);

    kheap_init();
    kprintf("[ok] Kernel heap at %p (64 KiB mapped, grows on demand)\n", (void *)HEAP_START);

    irq_init();
    kprintf("[ok] PIC remapped (IRQs at vectors 32-47)\n");

    timer_init(100);
    kprintf("[ok] PIT timer at %u Hz\n", timer_hz());

    keyboard_init();
    kprintf("[ok] Keyboard on IRQ 1\n");

    /* Everything is registered: let hardware interrupts in. */
    __asm__ volatile ("sti");
    kprintf("[ok] Interrupts enabled\n");

    kprintf("\nType 'help' for commands.\n");

    shell_run();
}
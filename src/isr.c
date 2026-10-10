#include "irq.h"
#include "isr.h"
#include "kprintf.h"
#include "vga.h"

static const char *exception_names[32] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

void isr_handler(struct registers *r) {
    /* Hardware interrupts (remapped to vectors 32-47) */
    if (r->int_no >= 32 && r->int_no < 48) {
        irq_dispatch(r);
        return;
    }
    /* Breakpoint is harmless: report it and carry on. This proves that
     * returning from an exception (iret) works. */
    if (r->int_no == 3) {
        kprintf("\n[breakpoint] at EIP=%08x\n", r->eip);
        return;
    }

    /* Everything else is fatal for now: print a report and halt. */
    term_set_color(0x4F);    /* white on red */

    kprintf("\n\n*** KERNEL EXCEPTION ***\n");
    kprintf("Exception %u: %s\n", r->int_no,
            r->int_no < 32 ? exception_names[r->int_no] : "Unknown");
    kprintf("Error code: %08x\n", r->err_code);
    kprintf("EIP=%08x  CS=%04x  EFLAGS=%08x\n", r->eip, r->cs, r->eflags);
    kprintf("EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x\n",
            r->eax, r->ebx, r->ecx, r->edx);
    kprintf("ESI=%08x  EDI=%08x  EBP=%08x\n", r->esi, r->edi, r->ebp);

    if (r->int_no == 14) {
        /* Page fault: CR2 holds the address that was accessed */
        uint32_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        kprintf("Faulting address: %08x (%s, %s, %s)\n", cr2,
                (r->err_code & 1) ? "protection violation" : "page not present",
                (r->err_code & 2) ? "write" : "read",
                (r->err_code & 4) ? "user mode" : "kernel mode");
    }

    kprintf("System halted.\n");
    for (;;)
        __asm__ volatile ("cli; hlt");
}
#include "idt.h"
#include <stdint.h>

/* One 8-byte gate descriptor */
struct idt_entry {
    uint16_t offset_low;     /* handler address, bits 0-15 */
    uint16_t selector;       /* code segment selector (our kernel CS = 0x08) */
    uint8_t  zero;
    uint8_t  type_attr;      /* 0x8E = present, ring 0, 32-bit interrupt gate */
    uint16_t offset_high;    /* handler address, bits 16-31 */
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

/* Array of the 32 stub addresses, defined in isr_stubs.s */
extern uint32_t isr_stub_table[];

static void idt_set_gate(int n, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = selector;
    idt[n].zero        = 0;
    idt[n].type_attr   = type_attr;
    idt[n].offset_high = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)(uintptr_t)&idt;

    /* CPU exceptions: vectors 0-31 */
    for (int i = 0; i < 32; i++)
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}
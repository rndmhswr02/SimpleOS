#include "irq.h"
#include "pic.h"

#define IRQ_BASE 32      /* IRQ 0 arrives as vector 32 */

static irq_handler_t handlers[16];

void irq_init(void) {
    pic_remap(IRQ_BASE, IRQ_BASE + 8);
}

void irq_register_handler(int irq, irq_handler_t handler) {
    handlers[irq] = handler;
    pic_unmask((uint8_t)irq);
}

void irq_unregister_handler(int irq) {
    pic_mask((uint8_t)irq);
    handlers[irq] = 0;
}

void irq_dispatch(struct registers *r) {
    int irq = (int)r->int_no - IRQ_BASE;

    if (pic_is_spurious((uint8_t)irq))
        return;                           /* ignore, and no EOI */

    /* Acknowledge first. The timer handler may switch to another task and not
     * return for a long time; without an EOI the PIC would never deliver another tick.
     * This is safe: interrupts stay disabled inside the handler. */
    pic_send_eoi((uint8_t)irq);

    if (handlers[irq])
        handlers[irq](r);
}
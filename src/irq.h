#ifndef IRQ_H
#define IRQ_H

#include "isr.h"      /* for struct registers */

typedef void (*irq_handler_t)(struct registers *r);

/* Remap the PIC (all IRQs stay masked until a handler is registered). */
void irq_init(void);

/* Install a handler for IRQ 0-15 and unmask that line. */
void irq_register_handler(int irq, irq_handler_t handler);

/* Mask the line and remove its handler. */
void irq_unregister_handler(int irq);

/* Called from isr_handler() for vectors 32-47. */
void irq_dispatch(struct registers *r);

#endif
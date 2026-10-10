#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* Remap IRQs 0-7 to vectors offset1.., and IRQs 8-15 to offset2... Masks everything. */
void pic_remap(uint8_t offset1, uint8_t offset2);

void pic_send_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);

/* Returns 1 if this IRQ is a spurious one (and should be ignored). */
int pic_is_spurious(uint8_t irq);

#endif
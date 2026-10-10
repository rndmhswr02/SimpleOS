#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/* Layout must match the order that isr_common pushes things, lowest address first. */
struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   /* from pushal */
    uint32_t int_no, err_code;                         /* from our stub */
    uint32_t eip, cs, eflags;                          /* from the CPU */
};

void isr_handler(struct registers *r);

#endif
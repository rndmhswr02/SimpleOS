#ifndef CPU_H
#define CPU_H

#include <stdint.h>

/* Disable interrupts and return the previous EFLAGS (which includes the IF bit). */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile ("pushfl\n\tpopl %0\n\tcli" : "=r"(flags) : : "memory");
    return flags;
}

/* Restore EFLAGS saved by irq_save(): interrupts come back on only if they were on before. */
static inline void irq_restore(uint32_t flags) {
    __asm__ volatile ("pushl %0\n\tpopfl" : : "r"(flags) : "memory", "cc");
}

#endif
#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Install the IRQ 1 handler. Call after irq_init(). */
void keyboard_init(void);

/* Blocks (halting the CPU) until a character is available, then returns it. */
char keyboard_getchar(void);

#endif
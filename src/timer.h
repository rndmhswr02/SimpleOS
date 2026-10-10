#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Start the PIT at `hz` ticks per second (valid range: 19 to 1193182). */
void timer_init(uint32_t hz);

uint32_t timer_ticks(void);     /* ticks since boot */
uint32_t timer_hz(void);        /* configured tick rate */

/* Sleep (halting the CPU) for about `ms` milliseconds.
 * Interrupts must be enabled, or this never returns. */
void timer_sleep_ms(uint32_t ms);

#endif
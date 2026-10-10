#include "timer.h"
#include "io.h"
#include "irq.h"

#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43
#define PIT_BASE_HZ   1193182u

static volatile uint32_t ticks = 0;   /* written by the IRQ handler, read elsewhere */
static uint32_t tick_hz = 0;

static void timer_irq(struct registers *r) {
    (void)r;
    ticks++;
}

void timer_init(uint32_t hz) {
    if (hz < 19)      hz = 19;                /* divisor must fit in 16 bits */
    if (hz > PIT_BASE_HZ) hz = PIT_BASE_HZ;
    tick_hz = hz;

    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor > 65535) divisor = 65535;

    outb(PIT_COMMAND, 0x36);                  /* channel 0, lobyte/hibyte, mode 3 (square wave) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_register_handler(0, timer_irq);       /* also unmasks IRQ 0 */
}

uint32_t timer_ticks(void) { return ticks; }
uint32_t timer_hz(void)    { return tick_hz; }

void timer_sleep_ms(uint32_t ms) {
    uint32_t wait  = (ms * tick_hz + 999) / 1000;   /* round up to whole ticks */
    uint32_t start = ticks;

    /* Unsigned subtraction stays correct even if the tick counter wraps around. */
    while ((uint32_t)(ticks - start) < wait)
        __asm__ volatile ("hlt");                   /* sleep until the next interrupt */
}
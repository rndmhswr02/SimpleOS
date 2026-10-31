#include "ulib.h"

__attribute__((section(".text.start")))
void _start(void) {
    volatile uint32_t x = 0;
    for (;;)
        x++;
}
#include "ulib.h"

__attribute__((section(".text.start")))
void _start(void) {
    print("About to write to kernel memory (0x00100000)...\n");
    *(volatile uint32_t *)0x00100000 = 0xDEADBEEF;
    print("This line should never be printed!\n");
    sys_exit(0);
}
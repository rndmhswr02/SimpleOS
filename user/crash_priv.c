#include "ulib.h"

__attribute__((section(".text.start")))
void _start(void) {
    print("About to execute a privileged instruction (cli)...\n");
    __asm__ volatile ("cli");
    print("This line should never be printed!\n");
    sys_exit(0);
}
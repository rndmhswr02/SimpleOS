#include "ulib.h"

__attribute__((section(".text.start")))
void _start(void) {
    print("Asking the kernel to print from kernel memory...\n");

    uint32_t r = sys_write(1, (const void *)0x00100000, 64);

    if (r == (uint32_t)-1)
        print("write() refused the kernel pointer (returned -1). Good.\n");
    else
        print("write() ACCEPTED a kernel pointer. That is a security hole!\n");

    sys_exit(0);
}
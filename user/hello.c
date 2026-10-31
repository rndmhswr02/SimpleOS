#include "ulib.h"

__attribute__((section(".text.start")))
void _start(void) {
    print("Hello from user mode! I am process ");
    print_uint(sys_getpid());
    print("\n");

    for (uint32_t i = 1; i <= 3; i++) {
        print("  tick ");
        print_uint(i);
        print(" (uptime ");
        print_uint(sys_ticks());
        print(" kernel ticks)\n");
        sys_sleep(1000);
    }

    print("Goodbye!\n");
    sys_exit(0);
}
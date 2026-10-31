.att_syntax prefix

.section .text
.global enter_usermode

# void enter_usermode(uint32_t entry, uint32_t user_esp)   (never returns)
#
# Builds the frame the CPU would have pushed for an interrupt that came from ring 3
# and executes iret, which "returns" into user mode.
enter_usermode:
    movl 4(%esp), %ecx        # entry point (user EIP)
    movl 8(%esp), %edx        # initial user stack pointer

    movw $0x23, %ax           # user data selector (index 4, RPL 3)
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    pushl $0x23               # SS
    pushl %edx                # ESP
    pushl $0x202              # EFLAGS: IF=1 (interrupts on) + the always-1 bit
    pushl $0x1B               # CS  (user code selector, RPL 3)
    pushl %ecx                # EIP
    iretl
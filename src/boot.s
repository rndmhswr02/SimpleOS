.intel_syntax noprefix

# Multiboot 1 header: QEMU's -kernel option looks for this
.set MAGIC,    0x1BADB002
.set FLAGS,    3                  # bit 0: align modules, bit 1: request memory map
.set CHECKSUM, -(MAGIC + FLAGS)   # MAGIC + FLAGS + CHECKSUM must equal 0

.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384                       # 16 KiB stack
stack_top:

.section .text
.global _start
_start:
    cli
    mov esp, offset stack_top     # set up the stack
    push ebx                      # arg 2: pointer to multiboot info
    push eax                      # arg 1: multiboot magic (0x2BADB002)
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
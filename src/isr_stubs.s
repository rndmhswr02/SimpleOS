.intel_syntax noprefix

# Stub for exceptions where the CPU does NOT push an error code
.macro ISR_NOERR num
.global isr\num
isr\num:
    push 0                  # dummy error code
    push \num               # vector number
    jmp isr_common
.endm

# Stub for exceptions where the CPU DOES push an error code
.macro ISR_ERR num
.global isr\num
isr\num:
    push \num               # vector number (error code already on stack)
    jmp isr_common
.endm

# Stub for hardware IRQs: IRQ number n arrives as vector `vec`
.macro IRQ n, vec
.global irq\n
irq\n:
    push 0                  # dummy error code
    push \vec               # vector number
    jmp isr_common
.endm

.section .text

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

# System call stub (vector 0x80 = 128). The pushes are written as raw bytes so the
# assembler can't pick an unexpected operand size:
#   6A 00           push 0       (dummy error code)
#   68 80 00 00 00  push 128     (vector number, 32-bit immediate)
.global isr128
isr128:
    .byte 0x6A, 0x00
    .byte 0x68
    .long 128
    jmp isr_common

# Common code for all stubs. At this point the stack holds (top to bottom):
#   vector number, error code, EIP, CS, EFLAGS
.extern isr_handler
isr_common:
    pushal                  # save eax, ecx, edx, ebx, esp, ebp, esi, edi

    mov ax, ds
    push eax                # save the data segment

    mov ax, 0x10            # switch to kernel data segments
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    cld                     # C code expects the direction flag clear
    push esp                # argument: pointer to struct registers
    call isr_handler
    add esp, 4

    pop eax                 # restore the data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popal
    add esp, 8              # drop vector number and error code
    iretd

# Tables of stub addresses, so C code can install them in a loop
.section .rodata
.global isr_stub_table
isr_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    .long isr\n
.endr

.global irq_stub_table
irq_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    .long irq\n
.endr
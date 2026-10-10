.att_syntax prefix

.section .text
.global gdt_flush

# void gdt_flush(uint32_t gdt_ptr_address)
gdt_flush:
    movl 4(%esp), %eax      # first argument: address of the gdt_ptr struct
    lgdt (%eax)

    movw $0x10, %ax         # kernel data selector
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    pushl $0x08             # kernel code selector (CS)
    pushl $.reload_cs       # return address (EIP)
    lretl                   # far return: pops EIP then CS, reloading CS

.reload_cs:
    ret
.intel_syntax noprefix
.code16                         # real mode
.global _start

_start:
    # Set up segments and stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              # stack grows downward from just below our code

    mov si, offset msg
.print_loop:
    lodsb                       # load byte at [si] into al, then si++
    or al, al                   # end of string?
    jz .halt
    mov ah, 0x0E                # BIOS teletype output
    mov bx, 0x0007              # page 0, light grey
    int 0x10
    jmp .print_loop

.halt:
    cli
    hlt
    jmp .halt

msg: .asciz "Hello from my boot sector!"

.org 510                        # pad up to byte 510
.word 0xAA55                    # boot signature (stored as 55 AA)
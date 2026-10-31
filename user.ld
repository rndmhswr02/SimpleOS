ENTRY(_start)

SECTIONS
{
    . = 0x40000000;                    /* USER_BASE */

    .text : {
        *(.text.start)                 /* _start must come first */
        *(.text .text.*)
    }
    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }
    .bss    : { *(COMMON) *(.bss .bss.*) }

    /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
MEMORY
{
    RAM (rwx) : ORIGIN = 0x42000000, LENGTH = 992M
}

SECTIONS
{
    . = 0x42000000;
    .text : {
        KEEP(*(.text.boot))
        *(.text)
    }
    PROVIDE(etext = .);
    .rodata : { *(.rodata) }
    PROVIDE(data = .);
    .data : { *(.data) }
    . = ALIGN(0x8);
    PROVIDE(bss_begin = .);
    .bss : { *(.bss) }
    PROVIDE(bss_end = .);
    PROVIDE(kernel_end = .);
}

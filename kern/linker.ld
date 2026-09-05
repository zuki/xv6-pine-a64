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
    PROVIDE(edata = .);
    .bss : { *(.bss) }
    PROVIDE(end = .);
}

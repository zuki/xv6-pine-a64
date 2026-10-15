ENTRY(_start)

SECTIONS
{
    . = 0xFFFF000042000000;
    _phys_start = 0x42000000;

    .boot : AT(_phys_start) {
        _boot_start = .;
        *(.boot)
        . = ALIGN(4096);
        _boot_pgtbl_start = .;
        . += 0x2000;
        _boot_end = .;
    }

    user_begin = .;
    .text.user : { obj/kern/user* (.text) }
    .rodata.user : { obj/kern/user* (.rodata) }
    .data.user : { obj/kern/user* (.data) }
    .bss.user : { obj/kern/user* (.bss) }
    user_end = .;

    .text : AT(_phys_start + (_boot_end - _boot_start) + (user_end - user_begin)) {
        *(.text)
    }
    PROVIDE(etext = .);
    .rodata : { *(.rodata) }
    PROVIDE(data = .);
    .data : { *(.data) }
    . = ALIGN(4096);
    PROVIDE(_bss_start = .);
    .bss : { *(.bss) }
    PROVIDE(_bss_end = .);
    PROVIDE(kernel_end = .);
}

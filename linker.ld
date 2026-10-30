OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_main)

SECTIONS
{
    . = 0xffffffff80000000;

    .limine_requests : ALIGN(8)
    {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    }

    .text : ALIGN(4096)
    {
        *(.text .text.*)
    }

    .rodata : ALIGN(4096)
    {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4096)
    {
        *(.data .data.*)
    }

    .bss : ALIGN(4096)
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
    }
}

/* MettaOS — script de enlace freestanding x86_64 (Multiboot2) */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    /* Carga física ~1 MiB (GRUB / Multiboot2) */
    . = 0x100000;
    __kernel_start = .;

    .multiboot_header : ALIGN(8)
    {
        KEEP(*(.multiboot_header))
    }

    .text : ALIGN(4K)
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    . = ALIGN(4K);
    __kernel_image_end = .;

    /*
     * Hueco de usuario 0x400000 .. 0x20000000.
     * El runtime de un AppImage ELF se enlaza en 0x400000; el BSS del kernel
     * no puede ocupar esa dirección.
     */
    . = 0x20000000;

    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(4K);
    __kernel_end = .;

    /*
     * libc, ld.so y el runtime del AppImage no caben bajo 0x400000:
     * esa página es la base fija del ELF ET_EXEC de Firefox.
     * VMA alta, otro PT_LOAD; el fichero no arrastra el hueco.
     */
    . = 0x30000000;
    .userblob : ALIGN(4K)
    {
        userblob_start = .;
        *(.userblob)
        userblob_end = .;
    }

    /DISCARD/ :
    {
        *(.note*)
        *(.eh_frame*)
        *(.comment)
    }
}
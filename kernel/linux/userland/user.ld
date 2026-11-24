ENTRY(_start)
PHDRS
{
    text PT_LOAD FLAGS(5);
}
SECTIONS
{
    /* 32 MiB: por encima del rodata del kernel y dentro del hueco de usuario. */
    . = 0x2000000;
    .text : {
        *(.text)
        *(.rodata)
    } :text
}

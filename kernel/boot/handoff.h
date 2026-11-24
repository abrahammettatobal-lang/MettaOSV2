/* MettaOS — construcción del handoff desde Multiboot2. */
#ifndef METTA_BOOT_HANDOFF_H
#define METTA_BOOT_HANDOFF_H

#include <stdint.h>

#include "metta/boot_info.h"

metta_status_t boot_build_info_from_mbi(uint32_t mb2_magic, const void *mbi,
                                        metta_boot_info_t *out);

/* Trampolín C desde el stub 64-bit (SysV: rdi=magic, rsi=mbi). */
void boot_enter(uint32_t mb2_magic, void *mbi) __attribute__((noreturn));

/* Puntero MB2 vivo solo durante kmain (SA-12 fb_init / tags). */
const void *boot_multiboot_info_raw(void);

#endif /* METTA_BOOT_HANDOFF_H */
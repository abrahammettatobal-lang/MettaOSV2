/* MettaOS — handoff boot → kmain (contrato §5). */
#ifndef METTA_BOOT_INFO_H
#define METTA_BOOT_INFO_H

#include "metta/types.h"

#define METTA_BOOT_MAGIC 0x4D455454415F424FULL /* "METTA_BO" */

#define METTA_BOOT_FLAG_FRAMEBUFFER  (1u << 0)
#define METTA_BOOT_FLAG_MEMMAP_VALID (1u << 1)

#define METTA_BOOT_MEMMAP_MAX 128u

typedef struct {
    metta_u64           magic;
    metta_u32           contract_rev;
    metta_u32           flags;
    metta_phys_addr_t   framebuffer_phys;
    metta_u32           fb_width;
    metta_u32           fb_height;
    metta_u32           fb_pitch;
    metta_u32           fb_bpp;
    metta_mem_region_t *memmap;
    metta_u32           memmap_count;
    metta_phys_addr_t   kernel_phys_start;
    metta_phys_addr_t   kernel_phys_end;
    metta_virt_addr_t   kernel_virt_base;
} metta_boot_info_t;

int boot_linux_selftest_enabled(void);

void kmain(metta_boot_info_t *info) __attribute__((noreturn));

metta_status_t boot_handoff_validate(const metta_boot_info_t *info);

#endif /* METTA_BOOT_INFO_H */
/* MettaOS — Multiboot2 → metta_boot_info_t (SA-04 / contrato rev 1). */
#include <stddef.h>
#include <stdint.h>

#include "metta/boot_info.h"
#include "handoff.h"

/* Símbolos absolutos definidos en linker.ld */
extern char __kernel_start[];
extern char __kernel_end[];

void kernel_halt(void) __attribute__((noreturn));

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289u

#define MB2_TAG_END        0u
#define MB2_TAG_LOAD_BASE  5u
#define MB2_TAG_MMAP       6u
#define MB2_TAG_FRAMEBUFFER 8u

struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

struct mb2_mmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
};

struct mb2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
};

static metta_boot_info_t   g_boot_info;
static metta_mem_region_t  g_memmap[METTA_BOOT_MEMMAP_MAX];
static const void         *g_raw_mbi;
static int g_linux_selftest;

int boot_linux_selftest_enabled(void) { return g_linux_selftest; }

static uint32_t align_up32(uint32_t v, uint32_t a)
{
    return (v + a - 1u) & ~(a - 1u);
}

static metta_u32 mb2_type_to_metta(uint32_t mb2_type)
{
    switch (mb2_type) {
    case 1:
        return METTA_MEM_AVAILABLE;
    case 2:
        return METTA_MEM_RESERVED;
    case 3:
        return METTA_MEM_ACPI_RECLAIMABLE;
    case 4:
        return METTA_MEM_NVS;
    case 5:
        return METTA_MEM_BAD;
    default:
        return METTA_MEM_RESERVED;
    }
}

metta_status_t boot_build_info_from_mbi(uint32_t mb2_magic, const void *mbi,
                                        metta_boot_info_t *out)
{
    uintptr_t kernel_load;
    bool      have_load_base;
    uint32_t  mmap_count;
    const uint8_t *cursor;
    uint32_t total_size;

    if (out == NULL || mbi == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (mb2_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        return METTA_ERR_CORRUPT;
    }

    kernel_load = (uintptr_t)(void *)__kernel_start;
    have_load_base = false;
    mmap_count = 0;

    for (size_t i = 0; i < METTA_BOOT_MEMMAP_MAX; i++) {
        g_memmap[i].base = 0;
        g_memmap[i].length = 0;
        g_memmap[i].type = 0;
        g_memmap[i].reserved = 0;
    }

    g_boot_info.magic = METTA_BOOT_MAGIC;
    g_boot_info.contract_rev = METTA_CONTRACT_REV;
    g_boot_info.flags = 0;
    g_boot_info.framebuffer_phys = 0;
    g_boot_info.fb_width = 0;
    g_boot_info.fb_height = 0;
    g_boot_info.fb_pitch = 0;
    g_boot_info.fb_bpp = 0;
    g_boot_info.memmap = g_memmap;
    g_boot_info.memmap_count = 0;
    g_boot_info.kernel_virt_base = METTA_KERNEL_VIRT_BASE;

    g_linux_selftest = 0;
    total_size = *(const uint32_t *)mbi;
    cursor = (const uint8_t *)mbi + 8;

    while ((uintptr_t)cursor < (uintptr_t)mbi + total_size) {
        const struct mb2_tag *tag = (const struct mb2_tag *)cursor;
        uint32_t size;

        if (tag->type == MB2_TAG_END) {
            break;
        }
        size = tag->size;
        if (size < sizeof(struct mb2_tag)) {
            return METTA_ERR_CORRUPT;
        }

        if (tag->type == 1 && size == sizeof(*tag) + sizeof("linux-selftest") &&
            size <= (size_t)((const uint8_t *)mbi + total_size - cursor)) {
            const char *arg = (const char *)(tag + 1);
            size_t i = 0;
            while (i < sizeof("linux-selftest") && arg[i] == "linux-selftest"[i]) ++i;
            g_linux_selftest = i == sizeof("linux-selftest");
        } else if (tag->type == MB2_TAG_LOAD_BASE && size >= 12) {
            kernel_load = *(const uint32_t *)((const uint8_t *)tag + 8);
            have_load_base = true;
        } else if (tag->type == MB2_TAG_MMAP) {
            const struct mb2_tag_mmap *mmap =
                (const struct mb2_tag_mmap *)tag;
            const uint8_t *ent =
                (const uint8_t *)tag + sizeof(struct mb2_tag_mmap);
            const uint8_t *end = (const uint8_t *)tag + size;

            if (mmap->entry_size < sizeof(struct mb2_mmap_entry)) {
                return METTA_ERR_CORRUPT;
            }

            while (ent + sizeof(struct mb2_mmap_entry) <= end &&
                   mmap_count < METTA_BOOT_MEMMAP_MAX) {
                const struct mb2_mmap_entry *raw =
                    (const struct mb2_mmap_entry *)ent;

                g_memmap[mmap_count].base = (metta_phys_addr_t)raw->base;
                g_memmap[mmap_count].length = raw->length;
                g_memmap[mmap_count].type = mb2_type_to_metta(raw->type);
                g_memmap[mmap_count].reserved = 0;
                mmap_count++;
                ent += mmap->entry_size;
            }
        } else if (tag->type == MB2_TAG_FRAMEBUFFER &&
                   size >= sizeof(struct mb2_tag_framebuffer)) {
            const struct mb2_tag_framebuffer *fb =
                (const struct mb2_tag_framebuffer *)tag;

            g_boot_info.framebuffer_phys = (metta_phys_addr_t)fb->addr;
            g_boot_info.fb_pitch = fb->pitch;
            g_boot_info.fb_width = fb->width;
            g_boot_info.fb_height = fb->height;
            g_boot_info.fb_bpp = fb->bpp;
            g_boot_info.flags |= METTA_BOOT_FLAG_FRAMEBUFFER;
        }

        cursor += align_up32(size, 8);
    }

    if (mmap_count > 0) {
        g_boot_info.memmap_count = mmap_count;
        g_boot_info.flags |= METTA_BOOT_FLAG_MEMMAP_VALID;
    }

    {
        uintptr_t ksz = (uintptr_t)(void *)__kernel_end -
                        (uintptr_t)(void *)__kernel_start;

        if (ksz == 0) {
            ksz = 64u * 1024u;
        }

        if (have_load_base) {
            g_boot_info.kernel_phys_start = (metta_phys_addr_t)kernel_load;
            g_boot_info.kernel_phys_end =
                (metta_phys_addr_t)(kernel_load + ksz);
        } else {
            g_boot_info.kernel_phys_start =
                (metta_phys_addr_t)(uintptr_t)(void *)__kernel_start;
            g_boot_info.kernel_phys_end =
                (metta_phys_addr_t)(uintptr_t)(void *)__kernel_start + ksz;
        }
    }

    *out = g_boot_info;
    return METTA_OK;
}

metta_status_t boot_handoff_validate(const metta_boot_info_t *info)
{
    if (info == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (info->magic != METTA_BOOT_MAGIC) {
        return METTA_ERR_CORRUPT;
    }
    if (info->contract_rev != METTA_CONTRACT_REV) {
        return METTA_ERR_CORRUPT;
    }
    if (info->memmap_count == 0 || info->memmap == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    if ((info->flags & METTA_BOOT_FLAG_FRAMEBUFFER) != 0) {
        if (info->fb_width == 0 || info->fb_height == 0 ||
            info->fb_pitch == 0 || info->fb_bpp == 0) {
            return METTA_ERR_INVALID_ARG;
        }
    }

    if (info->kernel_phys_start >= info->kernel_phys_end) {
        return METTA_ERR_CORRUPT;
    }

    return METTA_OK;
}

const void *boot_multiboot_info_raw(void)
{
    return g_raw_mbi;
}

void boot_enter(uint32_t mb2_magic, void *mbi)
{
    metta_status_t st;

    g_raw_mbi = mbi;
    st = boot_build_info_from_mbi(mb2_magic, mbi, &g_boot_info);
    if (st != METTA_OK) {
        kernel_halt();
    }

    kmain(&g_boot_info);
}
#include "cpu/gdt.h"
#include "cpu/segments.h"
#include "cpu/tss.h"

#define GDT_ACCESS_KCODE64 0x9Au
#define GDT_ACCESS_KDATA   0x92u
#define GDT_ACCESS_UCODE64 0xFAu /* DPL 3 */
#define GDT_ACCESS_UDATA   0xF2u /* DPL 3 */
#define GDT_FLAG_GRAN_4K   0x80u
#define GDT_FLAG_LONG      0x20u

static uint64_t gdt[GDT_ENTRY_COUNT];

static gdt_pointer_t gdt_ptr;

static void gdt_encode_flat(uint64_t *entry, uint8_t access, uint8_t flags)
{
    /* flags usa máscara estilo 0x20/0x80; el nibble G/D/L/AVL va en bits 52..55. */
    uint32_t limit = 0xFFFFF;
    uint8_t  flag_nibble = (uint8_t)(((flags | GDT_FLAG_GRAN_4K) >> 4) & 0x0Fu);
    uint64_t lo = 0;
    lo |= (uint64_t)(limit & 0xFFFFu);
    lo |= (uint64_t)((limit >> 16) & 0x0Fu) << 48;
    lo |= (uint64_t)flag_nibble << 52;
    lo |= (uint64_t)access << 40;
    *entry = lo;
}

static void gdt_encode_tss(uint64_t *lo, uint64_t *hi, uintptr_t base, uint32_t limit)
{
    /* Descriptor de sistema 16 bytes (SDM Vol.3): base 0:23 / 24:31 / 32:63. */
    *lo = 0;
    *lo |= (uint64_t)(limit & 0xFFFFu);
    *lo |= (uint64_t)(base & 0xFFFFu) << 16;
    *lo |= (uint64_t)((base >> 16) & 0xFFu) << 32;
    *lo |= (uint64_t)0x89u << 40; /* type=available TSS, present */
    *lo |= (uint64_t)((limit >> 16) & 0x0Fu) << 48;
    *lo |= (uint64_t)((base >> 24) & 0xFFu) << 56;

    *hi = (uint64_t)((base >> 32) & 0xFFFFFFFFu);
}

static void gdt_load_segments(void)
{
    __asm__ volatile(
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        : "m"(gdt_ptr)
        : "rax", "memory");
}

void gdt_install_tss_descriptor(void)
{
    tss64_t *tss = tss_get();
    uintptr_t base = (uintptr_t)tss;
    uint32_t limit = sizeof(tss64_t) - 1u;

    gdt_encode_tss(&gdt[5], &gdt[6], base, limit);
}

void gdt_init(void)
{
    gdt[0] = 0;
    gdt_encode_flat(&gdt[1], GDT_ACCESS_KCODE64, GDT_FLAG_LONG);
    gdt_encode_flat(&gdt[2], GDT_ACCESS_KDATA, 0);
    gdt_encode_flat(&gdt[3], GDT_ACCESS_UDATA, 0);
    gdt_encode_flat(&gdt[4], GDT_ACCESS_UCODE64, GDT_FLAG_LONG);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1u);
    gdt_ptr.base = (uint64_t)(uintptr_t)gdt;

    gdt_load_segments();
}

const gdt_pointer_t *gdt_pointer_get(void)
{
    return &gdt_ptr;
}
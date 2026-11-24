/*
 * MettaOS VMM (SA-08) — núcleo mínimo rev1.
 * HHDM, init/late-init y cambio CR3 para SA-09 (sustituye weak vmm_switch_to).
 * map/as_create siguen en weak de proc/ hasta extensión.
 */

#include "metta/vmm.h"
#include "metta/sync.h"
#include "metta/pmm.h"

#include <stddef.h>
#include <stdint.h>

extern metta_u8 pml4[METTA_PAGE_SIZE] __attribute__((aligned(METTA_PAGE_SIZE)));
extern metta_u8 pdpt[METTA_PAGE_SIZE] __attribute__((aligned(METTA_PAGE_SIZE)));
extern metta_u8 pd[METTA_PAGE_SIZE] __attribute__((aligned(METTA_PAGE_SIZE)));
extern metta_u8 pd_1g[METTA_PAGE_SIZE] __attribute__((aligned(METTA_PAGE_SIZE)));
extern metta_u8 pd_2g[METTA_PAGE_SIZE] __attribute__((aligned(METTA_PAGE_SIZE)));
extern metta_u8 pd_3g[METTA_PAGE_SIZE] __attribute__((aligned(METTA_PAGE_SIZE)));
extern metta_u8 pd_4g[METTA_PAGE_SIZE] __attribute__((aligned(METTA_PAGE_SIZE)));
extern metta_u8 pd_5g[METTA_PAGE_SIZE] __attribute__((aligned(METTA_PAGE_SIZE)));

#define VMM_PTE_PRESENT (1ULL << 0)
#define VMM_PTE_WRITE   (1ULL << 1)
#define VMM_PTE_USER    (1ULL << 2)
#define VMM_PTE_HUGE    (1ULL << 7)
#define VMM_PTE_PCD     (1ULL << 4) /* page-level cache disable (MMIO) */
#define VMM_PTE_PWT     (1ULL << 3)

#define VMM_INDEX_PML4(v) (((v) >> 39) & 0x1FFu)
#define VMM_HHDM_PML4_IDX VMM_INDEX_PML4(METTA_KERNEL_VIRT_BASE)
#define VMM_HHDM_MAP_BYTES METTA_PMM_MAX_PHYS_BYTES

/* Stubs uniprocesador (scheduler/proc enlazan aquí hasta SA-06). */
void metta_spin_lock(metta_spinlock_t *lock)
{
    if (lock != NULL) {
        lock->depth++;
    }
}

void metta_spin_unlock(metta_spinlock_t *lock)
{
    if (lock != NULL && lock->depth > 0) {
        lock->depth--;
    }
}

bool metta_spin_trylock(metta_spinlock_t *lock)
{
    if (lock == NULL) {
        return false;
    }
    metta_spin_lock(lock);
    return true;
}

metta_u32 metta_irq_save(void)
{
    return 0;
}

void metta_irq_restore(metta_u32 flags)
{
    (void)flags;
}

struct metta_address_space {
    metta_phys_addr_t pml4_phys;
    metta_virt_addr_t pml4_virt;
    metta_asid_t      asid;
};

static metta_address_space_t vmm_kernel_as;
static metta_address_space_t *vmm_current;
static metta_u8               vmm_ready;
static metta_u8               vmm_late_done;
static metta_spinlock_t       vmm_lock = METTA_SPINLOCK_INIT;

static metta_u64 vmm_read_cr3(void)
{
    metta_u64 cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static void vmm_write_cr3(metta_phys_addr_t phys)
{
    __asm__ volatile("mov %0, %%cr3" ::"r"(phys) : "memory");
}

static void vmm_invlpg(metta_virt_addr_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

static metta_u8 *vmm_boot_pd_for_gb(metta_u32 gb)
{
    switch (gb) {
    case 0:
        return pd;
    case 1:
        return pd_1g;
    case 2:
        return pd_2g;
    case 3:
        return pd_3g;
    case 4:
        return pd_4g;
    case 5:
        return pd_5g;
    default:
        return NULL;
    }
}

metta_status_t vmm_map_identity_range(metta_phys_addr_t phys, metta_u64 length)
{
    metta_phys_addr_t start;
    metta_phys_addr_t end;
    metta_phys_addr_t page;

    if (length == 0u) {
        return METTA_OK;
    }

    start = phys & ~((metta_phys_addr_t)(2u * 1024u * 1024u) - 1u);
    end   = (phys + length + (2u * 1024u * 1024u) - 1u) &
            ~((metta_phys_addr_t)(2u * 1024u * 1024u) - 1u);

    for (page = start; page < end; page += 2u * 1024u * 1024u) {
        metta_u32 gb  = (metta_u32)(page >> 30);
        metta_u32 idx = (metta_u32)((page >> 21) & 0x1FFu);
        metta_u8 *pd_tbl;
        metta_u64 *pte;

        if (gb > 5u) {
            return METTA_ERR_RANGE;
        }

        pd_tbl = vmm_boot_pd_for_gb(gb);
        if (pd_tbl == NULL) {
            return METTA_ERR_INTERNAL;
        }

        {
            metta_u64 *pdpte = (metta_u64 *)(void *)pdpt + gb;
            metta_u64  want  = ((metta_u64)(uintptr_t)pd_tbl & ~0xFFFULL) |
                              VMM_PTE_PRESENT | VMM_PTE_WRITE;
            if ((*pdpte & VMM_PTE_PRESENT) == 0) {
                *pdpte = want;
            }
        }

        pte  = (metta_u64 *)(void *)pd_tbl + idx;
        *pte = (page & ~((metta_u64)(2u * 1024u * 1024u) - 1u)) |
               VMM_PTE_PRESENT | VMM_PTE_WRITE | VMM_PTE_HUGE |
               VMM_PTE_PCD | VMM_PTE_PWT;
        vmm_invlpg((metta_virt_addr_t)page);
    }

    {
        metta_u64 *pml4e = (metta_u64 *)(void *)pml4;
        if ((*pml4e & VMM_PTE_PRESENT) == 0) {
            *pml4e = ((metta_u64)(uintptr_t)pdpt & ~0xFFFULL) |
                     VMM_PTE_PRESENT | VMM_PTE_WRITE;
        }
    }

    vmm_write_cr3((metta_phys_addr_t)(vmm_read_cr3() & ~0xFFFULL));
    return METTA_OK;
}

metta_status_t vmm_allow_user(metta_phys_addr_t phys, metta_u64 length)
{
    metta_phys_addr_t start;
    metta_phys_addr_t end;
    metta_phys_addr_t page;

    if (length == 0u) {
        return METTA_OK;
    }
    /* El kernel vive bajo 4 MiB (imagen) y desde 512 MiB (BSS). */
    if (phys < 0x400000u || phys + length < phys) {
        return METTA_ERR_RANGE;
    }

    start = phys & ~((metta_phys_addr_t)(2u * 1024u * 1024u) - 1u);
    end   = (phys + length + (2u * 1024u * 1024u) - 1u) &
            ~((metta_phys_addr_t)(2u * 1024u * 1024u) - 1u);

    for (page = start; page < end; page += 2u * 1024u * 1024u) {
        metta_u32 gb  = (metta_u32)(page >> 30);
        metta_u32 idx = (metta_u32)((page >> 21) & 0x1FFu);
        metta_u8 *pd_tbl;
        metta_u64 *pte;

        if (gb > 5u) {
            return METTA_ERR_RANGE;
        }
        pd_tbl = vmm_boot_pd_for_gb(gb);
        if (pd_tbl == NULL) {
            return METTA_ERR_INTERNAL;
        }
        pte = (metta_u64 *)(void *)pd_tbl + idx;
        *pte |= VMM_PTE_USER;
        vmm_invlpg((metta_virt_addr_t)page);
    }

    /* El acceso de ring 3 exige U/S=1 en PML4 y PDPT, no solo en la hoja. */
    {
        metta_u64 *pml4e = (metta_u64 *)(void *)pml4;
        metta_u64 *pdpte = (metta_u64 *)(void *)pdpt;
        metta_u32 i;

        pml4e[0] |= VMM_PTE_USER;
        for (i = 0; i < 6u; i++) {
            if (pdpte[i] & VMM_PTE_PRESENT) {
                pdpte[i] |= VMM_PTE_USER;
            }
        }
    }

    vmm_write_cr3((metta_phys_addr_t)(vmm_read_cr3() & ~0xFFFULL));
    return METTA_OK;
}

static metta_virt_addr_t vmm_table_va(metta_phys_addr_t phys)
{
    if (!vmm_late_done) {
        return (metta_virt_addr_t)phys;
    }
    return (metta_virt_addr_t)(METTA_KERNEL_VIRT_BASE + phys);
}

static metta_u64 *vmm_pte(metta_virt_addr_t table, metta_u32 index)
{
    return (metta_u64 *)(uintptr_t)(table + index * sizeof(metta_u64));
}

static void vmm_zero_page(metta_phys_addr_t phys)
{
    metta_u64 *pg = (metta_u64 *)(uintptr_t)vmm_table_va(phys);
    for (metta_u32 i = 0; i < METTA_PAGE_SIZE / sizeof(metta_u64); i++) {
        pg[i] = 0;
    }
}

static metta_phys_addr_t vmm_alloc_table(void)
{
    metta_phys_frame_t *frame = NULL;

    if (pmm_frame_alloc(METTA_FRAME_PAGE_TABLE, &frame) != METTA_OK || frame == NULL) {
        return METTA_PHYS_NONE;
    }
    metta_phys_addr_t phys = pmm_frame_phys(frame);
    vmm_zero_page(phys);
    return phys;
}

static metta_status_t vmm_install_hhdm(metta_address_space_t *as)
{
    /*
     * HHDM completo (6 GiB de tablas) requiere frames PMM seguros.
     * Hasta enlace high-half, el identity map de boot ya cubre 0..6 GiB;
     * instalar HHDM aquí es opcional y se omite para no tocar CR3 en frío.
     */
    (void)as;
    (void)VMM_HHDM_PML4_IDX;
    (void)vmm_alloc_table;
    return METTA_OK;
}

static void vmm_drop_identity_low(metta_address_space_t *as)
{
    metta_u64 *pml4e = vmm_pte(as->pml4_virt, 0);
    if ((*pml4e & VMM_PTE_PRESENT) == 0) {
        return;
    }

    *pml4e = 0;
    vmm_write_cr3(as->pml4_phys);

    for (metta_virt_addr_t va = 0; va < (256ULL << 20); va += METTA_PAGE_SIZE) {
        vmm_invlpg(va);
    }
}

metta_status_t vmm_init(void)
{
    if (vmm_ready) {
        return METTA_OK;
    }

    metta_spin_lock(&vmm_lock);

    vmm_kernel_as.pml4_phys = (metta_phys_addr_t)(vmm_read_cr3() & ~0xFFFULL);
    vmm_kernel_as.pml4_virt = (metta_virt_addr_t)(uintptr_t)pml4;
    vmm_kernel_as.asid      = METTA_ASID_KERNEL;

    metta_status_t st = vmm_install_hhdm(&vmm_kernel_as);
    if (st != METTA_OK) {
        metta_spin_unlock(&vmm_lock);
        return st;
    }

    vmm_write_cr3(vmm_kernel_as.pml4_phys);
    vmm_current = &vmm_kernel_as;
    vmm_ready   = 1;

    metta_spin_unlock(&vmm_lock);
    return METTA_OK;
}

metta_status_t vmm_late_init_after_pmm(void)
{
    metta_u64 rip;

    if (!vmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (vmm_late_done) {
        return METTA_OK;
    }

    metta_spin_lock(&vmm_lock);

    __asm__ volatile("lea (%%rip), %0" : "=r"(rip));
    vmm_late_done = 1;

    /*
     * boot-map: retirar identidad baja solo con kernel en canonical high.
     * Enlazado bajo (0x100000) se omite el desmapeo hasta enlace high-half.
     */
    if (rip >= METTA_KERNEL_VIRT_BASE) {
        vmm_drop_identity_low(&vmm_kernel_as);
    }

    metta_spin_unlock(&vmm_lock);
    return METTA_OK;
}

metta_status_t vmm_switch_to(metta_address_space_t *as)
{
    if (as == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (!vmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (as->pml4_phys == METTA_PHYS_NONE) {
        return METTA_ERR_CORRUPT;
    }

    metta_spin_lock(&vmm_lock);
    vmm_write_cr3(as->pml4_phys);
    vmm_current = as;
    metta_spin_unlock(&vmm_lock);

    return METTA_OK;
}

metta_address_space_t *vmm_current_as(void)
{
    return vmm_current;
}
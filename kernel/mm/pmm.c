#include "metta/pmm.h"

#include <stdint.h>

#define PMM_BITMAP_BYTES  (METTA_PMM_MAX_FRAMES / 8u)

typedef struct metta_phys_frame {
    metta_u32 index;
} metta_phys_frame_s;

typedef struct {
    metta_u32           refcnt;
    metta_frame_class_t class;
    metta_u8            allocated;
} pmm_slot_t;

/*
 * Bit 1 = no en free list (ocupado / reserva).
 * ~192 KiB para 6 GiB.
 */
static metta_u8 pmm_bitmap[PMM_BITMAP_BYTES];

static metta_phys_frame_s pmm_handles[METTA_PMM_MAX_FRAMES];
static pmm_slot_t           pmm_slots[METTA_PMM_MAX_FRAMES];

static metta_u64 pmm_total;
static metta_u64 pmm_free_count;
static metta_u64 pmm_scan_hint;
static metta_u32 pmm_ready;
static volatile metta_u32 pmm_lock_flag;

static void pmm_panic_contract(void)
{
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void pmm_lock(void)
{
    while (__sync_lock_test_and_set(&pmm_lock_flag, 1u) != 0u) {
        __asm__ volatile("pause");
    }
}

static void pmm_unlock(void)
{
    __sync_lock_release(&pmm_lock_flag);
}

static int pmm_frame_index(metta_phys_addr_t phys, metta_u64 *index_out)
{
    if (phys == METTA_PHYS_NONE || (phys & (METTA_PAGE_SIZE - 1u)) != 0u) {
        return 0;
    }
    if (phys >= METTA_PMM_MAX_PHYS_BYTES) {
        return 0;
    }
    *index_out = phys >> METTA_PAGE_SHIFT;
    return 1;
}

static int pmm_bitmap_test(metta_u64 index)
{
    metta_u64 byte = index >> 3;
    metta_u8  bit  = (metta_u8)(index & 7u);
    return (pmm_bitmap[byte] >> bit) & 1u;
}

static void pmm_bitmap_set(metta_u64 index)
{
    metta_u64 byte = index >> 3;
    metta_u8  bit  = (metta_u8)(index & 7u);
    pmm_bitmap[byte] |= (metta_u8)(1u << bit);
}

static void pmm_bitmap_clear(metta_u64 index)
{
    metta_u64 byte = index >> 3;
    metta_u8  bit  = (metta_u8)(index & 7u);
    pmm_bitmap[byte] &= (metta_u8)~(1u << bit);
}

static metta_frame_class_t pmm_class_for_region_type(metta_u32 type)
{
    switch (type) {
    case METTA_MEM_KERNEL_IMAGE:
        return METTA_FRAME_KERNEL_STATIC;
    case METTA_MEM_FRAMEBUFFER:
        return METTA_FRAME_RESERVED;
    default:
        return METTA_FRAME_RESERVED;
    }
}

static int pmm_class_may_free(metta_frame_class_t class)
{
    switch (class) {
    case METTA_FRAME_KERNEL_STATIC:
    case METTA_FRAME_RESERVED:
    case METTA_FRAME_DMA:
        return 0;
    default:
        return 1;
    }
}

static void pmm_apply_region_used(metta_phys_addr_t base, metta_u64 length,
                                  metta_frame_class_t class)
{
    if (length == 0u) {
        return;
    }

    metta_u64 start = base & ~(metta_u64)(METTA_PAGE_SIZE - 1u);
    metta_u64 end   = base + length;
    if (end > METTA_PMM_MAX_PHYS_BYTES) {
        end = METTA_PMM_MAX_PHYS_BYTES;
    }
    if (end <= start) {
        return;
    }

    metta_u64 frame_start = start >> METTA_PAGE_SHIFT;
    metta_u64 frame_end   = (end + METTA_PAGE_SIZE - 1u) >> METTA_PAGE_SHIFT;

    for (metta_u64 f = frame_start; f < frame_end && f < METTA_PMM_MAX_FRAMES; f++) {
        if (pmm_ready && !pmm_bitmap_test(f))
            pmm_free_count--;
        pmm_bitmap_set(f);
        pmm_slots[f].refcnt     = (class == METTA_FRAME_KERNEL_STATIC) ? UINT32_MAX : 0u;
        pmm_slots[f].class      = class;
        pmm_slots[f].allocated  = 1u;
    }
}

static void pmm_apply_region_free(metta_phys_addr_t base, metta_u64 length)
{
    if (length == 0u) {
        return;
    }

    metta_u64 start = base & ~(metta_u64)(METTA_PAGE_SIZE - 1u);
    metta_u64 end   = base + length;
    if (end > METTA_PMM_MAX_PHYS_BYTES) {
        end = METTA_PMM_MAX_PHYS_BYTES;
    }
    if (end <= start) {
        return;
    }

    if (start < METTA_PHYS_MIN_ALLOC_BYTES) {
        if (end <= METTA_PHYS_MIN_ALLOC_BYTES) {
            return;
        }
        length = end - METTA_PHYS_MIN_ALLOC_BYTES;
        start  = METTA_PHYS_MIN_ALLOC_BYTES;
    }

    metta_u64 frame_start = start >> METTA_PAGE_SHIFT;
    metta_u64 frame_end   = (start + length + METTA_PAGE_SIZE - 1u) >> METTA_PAGE_SHIFT;

    for (metta_u64 f = frame_start; f < frame_end && f < METTA_PMM_MAX_FRAMES; f++) {
        if (pmm_bitmap_test(f) != 0) {
            pmm_bitmap_clear(f);
            pmm_slots[f].refcnt    = 0u;
            pmm_slots[f].class     = METTA_FRAME_RESERVED;
            pmm_slots[f].allocated = 0u;
        }
    }
}

static void pmm_reserve_owned_storage(void)
{
    extern char __kernel_start[], __kernel_image_end[], __bss_start[], __bss_end[];
    pmm_apply_region_used((uintptr_t)__kernel_start, (uintptr_t)__kernel_image_end - (uintptr_t)__kernel_start,
                          METTA_FRAME_KERNEL_STATIC);
    pmm_apply_region_used((uintptr_t)__bss_start, (uintptr_t)__bss_end - (uintptr_t)__bss_start,
                          METTA_FRAME_KERNEL_STATIC);
}

static void pmm_recount_free(void)
{
    metta_u64 free_n = 0;
    for (metta_u64 f = 0; f < METTA_PMM_MAX_FRAMES; f++) {
        if (pmm_bitmap_test(f) == 0) {
            free_n++;
        }
    }
    pmm_free_count = free_n;
}

static metta_status_t pmm_release_index_locked(metta_u32 idx)
{
    if (idx >= METTA_PMM_MAX_FRAMES) {
        return METTA_ERR_INVALID_ARG;
    }
    if (!pmm_class_may_free(pmm_slots[idx].class)) {
        return METTA_ERR_PERM;
    }
    if (pmm_slots[idx].allocated == 0u) {
        return METTA_ERR_INVALID_ARG;
    }
    if (pmm_slots[idx].refcnt != 0u) {
        return METTA_ERR_BUSY;
    }
    if (pmm_bitmap_test(idx) == 0) {
        return METTA_ERR_CORRUPT;
    }

    pmm_slots[idx].allocated = 0u;
    pmm_slots[idx].class     = METTA_FRAME_RESERVED;
    pmm_bitmap_clear(idx);
    pmm_free_count++;
    if (idx < pmm_scan_hint) {
        pmm_scan_hint = idx;
    }

    return METTA_OK;
}

metta_status_t pmm_init(const metta_mem_region_t *regions, metta_u32 count)
{
    if (regions == NULL || count == 0u) {
        return METTA_ERR_INVALID_ARG;
    }

    pmm_ready      = 0u;
    pmm_scan_hint  = 0;
    pmm_total      = METTA_PMM_MAX_FRAMES;
    pmm_free_count = 0;

    for (metta_u64 f = 0; f < METTA_PMM_MAX_FRAMES; f++) {
        pmm_handles[f].index     = (metta_u32)f;
        pmm_slots[f].refcnt      = 0u;
        pmm_slots[f].class       = METTA_FRAME_RESERVED;
        pmm_slots[f].allocated   = 0u;
    }

    for (size_t i = 0; i < PMM_BITMAP_BYTES; i++) {
        pmm_bitmap[i] = 0xFFu;
    }

    /* Boot-map: legacy < 1 MiB siempre fuera del allocator. */
    pmm_apply_region_used(0, METTA_PHYS_MIN_ALLOC_BYTES, METTA_FRAME_RESERVED);

    for (metta_u32 i = 0; i < count; i++) {
        const metta_mem_region_t *r = &regions[i];
        if (r->length == 0u) {
            continue;
        }

        if (r->type == METTA_MEM_AVAILABLE) {
            pmm_apply_region_free(r->base, r->length);
        } else if (r->type == METTA_MEM_BAD) {
            pmm_apply_region_used(r->base, r->length, METTA_FRAME_RESERVED);
        } else {
            metta_frame_class_t cls = pmm_class_for_region_type(r->type);
            pmm_apply_region_used(r->base, r->length, cls);
        }
    }

    pmm_reserve_owned_storage();
    pmm_recount_free();
    pmm_ready = 1u;

    return METTA_OK;
}

metta_status_t pmm_mark_reserved(metta_phys_addr_t base, metta_u64 length)
{
    metta_u64 unused;

    if (!pmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (length == 0u || !pmm_frame_index(base, &unused)) {
        return METTA_ERR_RANGE;
    }

    pmm_lock();
    pmm_apply_region_used(base, length, METTA_FRAME_RESERVED);
    pmm_unlock();

    return METTA_OK;
}

metta_status_t pmm_frame_alloc(metta_frame_class_t class, metta_phys_frame_t **out)
{
    if (out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (class == METTA_FRAME_KERNEL_STATIC || class == METTA_FRAME_RESERVED
        || class == METTA_FRAME_DMA) {
        return METTA_ERR_INVALID_ARG;
    }
    if (!pmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }

    pmm_lock();

    if (pmm_free_count == 0u) {
        pmm_unlock();
        return METTA_ERR_NO_MEMORY;
    }

    metta_u64 start = pmm_scan_hint;
    metta_phys_frame_t *found = NULL;

    for (metta_u64 n = 0; n < METTA_PMM_MAX_FRAMES; n++) {
        metta_u64 f = (start + n) % METTA_PMM_MAX_FRAMES;

        if (pmm_bitmap_test(f) != 0) {
            continue;
        }
        if (pmm_slots[f].allocated != 0u) {
            pmm_unlock();
            return METTA_ERR_CORRUPT;
        }

        pmm_bitmap_set(f);
        pmm_slots[f].refcnt    = 1u;
        pmm_slots[f].class     = class;
        pmm_slots[f].allocated = 1u;
        pmm_free_count--;
        pmm_scan_hint = (f + 1u) % METTA_PMM_MAX_FRAMES;
        found         = &pmm_handles[f];
        break;
    }

    pmm_unlock();

    if (found == NULL) {
        return METTA_ERR_NO_MEMORY;
    }

    *out = found;
    return METTA_OK;
}

metta_status_t pmm_frame_free(metta_phys_frame_t *frame)
{
    if (!pmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (frame == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    pmm_lock();

    metta_u32 idx = frame->index;
    if (idx >= METTA_PMM_MAX_FRAMES) {
        pmm_unlock();
        return METTA_ERR_INVALID_ARG;
    }

    if (!pmm_class_may_free(pmm_slots[idx].class)) {
        pmm_unlock();
        return METTA_ERR_PERM;
    }

    if (pmm_slots[idx].refcnt > 1u) {
        pmm_unlock();
        return METTA_ERR_BUSY;
    }
    if (pmm_slots[idx].refcnt == 0u || pmm_slots[idx].allocated == 0u) {
        pmm_unlock();
        return METTA_ERR_INVALID_ARG;
    }

    pmm_slots[idx].refcnt = 0u;
    metta_status_t st     = pmm_release_index_locked(idx);

    pmm_unlock();
    return st;
}

metta_status_t pmm_frame_get(metta_phys_frame_t *frame, metta_frame_desc_t *out)
{
    if (frame == NULL || out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (!pmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }

    metta_u32 idx = frame->index;
    if (idx >= METTA_PMM_MAX_FRAMES || pmm_slots[idx].allocated == 0u) {
        return METTA_ERR_INVALID_ARG;
    }

    out->addr   = (metta_phys_addr_t)(idx << METTA_PAGE_SHIFT);
    out->class  = pmm_slots[idx].class;
    out->refcnt = pmm_slots[idx].refcnt;
    return METTA_OK;
}

metta_phys_addr_t pmm_frame_phys(metta_phys_frame_t *frame)
{
    if (frame == NULL || !pmm_ready || frame->index >= METTA_PMM_MAX_FRAMES) {
        return METTA_PHYS_NONE;
    }
    return (metta_phys_addr_t)(frame->index << METTA_PAGE_SHIFT);
}

metta_status_t pmm_frame_ref_inc(metta_phys_frame_t *frame)
{
    if (!pmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (frame == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    pmm_lock();
    metta_u32 idx = frame->index;
    if (idx >= METTA_PMM_MAX_FRAMES || pmm_slots[idx].allocated == 0u) {
        pmm_unlock();
        return METTA_ERR_INVALID_ARG;
    }
    if (pmm_slots[idx].class == METTA_FRAME_KERNEL_STATIC) {
        pmm_unlock();
        return METTA_ERR_PERM;
    }
    if (pmm_slots[idx].refcnt == UINT32_MAX) {
        pmm_unlock();
        return METTA_ERR_INTERNAL;
    }

    pmm_slots[idx].refcnt++;
    pmm_unlock();
    return METTA_OK;
}

metta_status_t pmm_frame_ref_dec(metta_phys_frame_t *frame)
{
    if (!pmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (frame == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    pmm_lock();
    metta_u32 idx = frame->index;
    if (idx >= METTA_PMM_MAX_FRAMES || pmm_slots[idx].allocated == 0u) {
        pmm_unlock();
        return METTA_ERR_INVALID_ARG;
    }
    if (pmm_slots[idx].refcnt == 0u) {
        pmm_unlock();
        pmm_panic_contract();
        return METTA_ERR_CORRUPT;
    }
    if (pmm_slots[idx].refcnt == UINT32_MAX) {
        pmm_unlock();
        return METTA_ERR_PERM;
    }

    pmm_slots[idx].refcnt--;

    metta_status_t st = METTA_OK;
    if (pmm_slots[idx].refcnt == 0u && pmm_class_may_free(pmm_slots[idx].class)) {
        st = pmm_release_index_locked(idx);
    }

    pmm_unlock();
    return st;
}

metta_status_t pmm_query_stats(metta_u64 *free_frames, metta_u64 *total_frames)
{
    if (!pmm_ready) {
        return METTA_ERR_UNINITIALIZED;
    }

    if (free_frames != NULL) {
        *free_frames = pmm_free_count;
    }
    if (total_frames != NULL) {
        *total_frames = pmm_total;
    }
    return METTA_OK;
}

#ifndef METTA_PMM_H
#define METTA_PMM_H

#include "metta/types.h"

/* Cobertura del bitmap (QEMU 6144 MiB, granularidad 4 KiB). */
#define METTA_PMM_MAX_PHYS_BYTES  (6144ULL * 1024ULL * 1024ULL)
#define METTA_PMM_MAX_FRAMES      (METTA_PMM_MAX_PHYS_BYTES / METTA_PAGE_SIZE)
#define METTA_PHYS_MIN_ALLOC_BYTES  0x100000ULL  /* <1 MiB reservado (boot-map) */

typedef struct metta_phys_frame metta_phys_frame_t;

typedef metta_u64 metta_frame_ref_t;

typedef enum {
    METTA_FRAME_KERNEL_STATIC = 0,
    METTA_FRAME_KERNEL_HEAP,
    METTA_FRAME_PAGE_TABLE,
    METTA_FRAME_USER,
    METTA_FRAME_DMA,
    METTA_FRAME_RESERVED
} metta_frame_class_t;

typedef struct {
    metta_phys_addr_t   addr;
    metta_frame_class_t class;
    metta_u32           refcnt;
} metta_frame_desc_t;

metta_status_t pmm_init(const metta_mem_region_t *regions, metta_u32 count);

metta_status_t pmm_frame_alloc(metta_frame_class_t class, metta_phys_frame_t **out);
metta_status_t pmm_frame_free(metta_phys_frame_t *frame);

metta_status_t pmm_frame_get(metta_phys_frame_t *frame, metta_frame_desc_t *out);
metta_phys_addr_t pmm_frame_phys(metta_phys_frame_t *frame);

metta_status_t pmm_frame_ref_inc(metta_phys_frame_t *frame);
metta_status_t pmm_frame_ref_dec(metta_phys_frame_t *frame);

metta_status_t pmm_mark_reserved(metta_phys_addr_t base, metta_u64 length);
metta_status_t pmm_query_stats(metta_u64 *free_frames, metta_u64 *total_frames);

#endif /* METTA_PMM_H */
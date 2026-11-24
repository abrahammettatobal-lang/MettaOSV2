#ifndef METTA_BLOCK_H
#define METTA_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#include "metta/types.h"

/** Tamaño de sector lógico (compatible MFS / particiones PC). */
#define METTA_BLK_SECTOR_SIZE   512u
#define METTA_BLK_MAX_DEVICES   8u

typedef uint64_t metta_blk_lba_t;

typedef struct metta_block_dev metta_block_dev_t;

typedef metta_status_t (*metta_blk_rw_fn)(metta_block_dev_t *dev,
                                            metta_blk_lba_t     lba,
                                            void               *buf,
                                            uint32_t            count);

struct metta_block_dev {
    const char         *name;
    uint64_t            sector_count;
    metta_blk_rw_fn     read_sectors;
    metta_blk_rw_fn     write_sectors;
    void               *driver_data;
};

metta_status_t block_init(void);

metta_status_t block_register(metta_block_dev_t *dev);
metta_status_t block_unregister(const char *name);

metta_block_dev_t *block_lookup(const char *name);

metta_status_t block_read(metta_block_dev_t *dev,
                          metta_blk_lba_t    lba,
                          void              *buf,
                          uint32_t           count);

metta_status_t block_write(metta_block_dev_t *dev,
                           metta_blk_lba_t    lba,
                           const void        *buf,
                           uint32_t           count);

#endif /* METTA_BLOCK_H */
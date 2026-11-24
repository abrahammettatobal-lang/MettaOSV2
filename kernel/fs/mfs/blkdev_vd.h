#ifndef MFS_BLKDEV_VD_H
#define MFS_BLKDEV_VD_H

#include "blkdev.h"

#include "../../drivers/block/block.h"

/** Adapta un `metta_block_dev_t` (sectores 512 B) a `mfs_blkdev_t` (bloques 4 KiB). */
int mfs_blkdev_bind_block(mfs_blkdev_t *out, metta_block_dev_t *bdev);

#endif /* MFS_BLKDEV_VD_H */
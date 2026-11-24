#ifndef MFS_BLKDEV_H
#define MFS_BLKDEV_H

#include <stddef.h>
#include <stdint.h>

#include "mfs_format.h"

#define MFS_DEV_OK        0
#define MFS_DEV_EINVAL   -1
#define MFS_DEV_IOERR    -2
#define MFS_DEV_RANGE    -3

typedef struct mfs_blkdev mfs_blkdev_t;

typedef int (*mfs_blkdev_read_fn)(mfs_blkdev_t *dev, uint32_t lba, void *buf);
typedef int (*mfs_blkdev_write_fn)(mfs_blkdev_t *dev, uint32_t lba, const void *buf);

struct mfs_blkdev {
    mfs_blkdev_read_fn  read;
    mfs_blkdev_write_fn write;
    void               *priv;
    uint32_t            block_count;
    uint32_t            block_size;
};

int mfs_blkdev_read(mfs_blkdev_t *dev, uint32_t lba, void *buf);
int mfs_blkdev_write(mfs_blkdev_t *dev, uint32_t lba, const void *buf);

/* Persistencia en RAM — volumen lineal para QEMU / desarrollo. */
int mfs_blkdev_stub_init(mfs_blkdev_t *dev, uint32_t block_count);
void mfs_blkdev_stub_wipe(mfs_blkdev_t *dev);

#endif /* MFS_BLKDEV_H */
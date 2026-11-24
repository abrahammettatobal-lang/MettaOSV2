#include "blkdev.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *base;
    uint32_t block_count;
    uint32_t block_size;
} mfs_stub_priv_t;

static uint8_t mfs_stub_storage[MFS_STUB_BLOCK_COUNT][MFS_BLOCK_SIZE];

static int stub_read(mfs_blkdev_t *dev, uint32_t lba, void *buf)
{
    mfs_stub_priv_t *p = (mfs_stub_priv_t *)dev->priv;

    if (dev == NULL || buf == NULL || p == NULL) {
        return MFS_DEV_EINVAL;
    }
    if (lba >= p->block_count) {
        return MFS_DEV_RANGE;
    }

    const uint8_t *src = p->base + (size_t)lba * (size_t)p->block_size;
    uint8_t *dst = (uint8_t *)buf;

    for (uint32_t i = 0; i < p->block_size; i++) {
        dst[i] = src[i];
    }
    return MFS_DEV_OK;
}

static int stub_write(mfs_blkdev_t *dev, uint32_t lba, const void *buf)
{
    mfs_stub_priv_t *p = (mfs_stub_priv_t *)dev->priv;

    if (dev == NULL || buf == NULL || p == NULL) {
        return MFS_DEV_EINVAL;
    }
    if (lba >= p->block_count) {
        return MFS_DEV_RANGE;
    }

    uint8_t *dst = p->base + (size_t)lba * (size_t)p->block_size;
    const uint8_t *src = (const uint8_t *)buf;

    for (uint32_t i = 0; i < p->block_size; i++) {
        dst[i] = src[i];
    }
    return MFS_DEV_OK;
}

static mfs_stub_priv_t mfs_stub_priv;

int mfs_blkdev_read(mfs_blkdev_t *dev, uint32_t lba, void *buf)
{
    if (dev == NULL || dev->read == NULL || buf == NULL) {
        return MFS_DEV_EINVAL;
    }
    return dev->read(dev, lba, buf);
}

int mfs_blkdev_write(mfs_blkdev_t *dev, uint32_t lba, const void *buf)
{
    if (dev == NULL || dev->write == NULL || buf == NULL) {
        return MFS_DEV_EINVAL;
    }
    return dev->write(dev, lba, buf);
}

int mfs_blkdev_stub_init(mfs_blkdev_t *dev, uint32_t block_count)
{
    if (dev == NULL || block_count == 0u || block_count > MFS_STUB_BLOCK_COUNT) {
        return MFS_DEV_EINVAL;
    }

    mfs_stub_priv.base = &mfs_stub_storage[0][0];
    mfs_stub_priv.block_count = block_count;
    mfs_stub_priv.block_size = MFS_BLOCK_SIZE;

    dev->read = stub_read;
    dev->write = stub_write;
    dev->priv = &mfs_stub_priv;
    dev->block_count = block_count;
    dev->block_size = MFS_BLOCK_SIZE;

    mfs_blkdev_stub_wipe(dev);
    return MFS_DEV_OK;
}

void mfs_blkdev_stub_wipe(mfs_blkdev_t *dev)
{
    mfs_stub_priv_t *p;

    if (dev == NULL || dev->priv == NULL) {
        return;
    }

    p = (mfs_stub_priv_t *)dev->priv;
    size_t bytes = (size_t)p->block_count * (size_t)p->block_size;
    uint8_t *b = p->base;

    for (size_t i = 0; i < bytes; i++) {
        b[i] = 0;
    }
}
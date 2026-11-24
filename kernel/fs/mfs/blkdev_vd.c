#include "blkdev_vd.h"

#define MFS_SECTORS_PER_BLOCK  (MFS_BLOCK_SIZE / METTA_BLK_SECTOR_SIZE)

typedef struct {
    metta_block_dev_t *bdev;
} mfs_vd_priv_t;

static mfs_vd_priv_t mfs_vd_priv;

static int mfs_vd_read(mfs_blkdev_t *dev, uint32_t lba, void *buf)
{
    mfs_vd_priv_t *p;
    metta_blk_lba_t sector;
    metta_status_t st;

    if (dev == NULL || buf == NULL) {
        return MFS_DEV_EINVAL;
    }
    p = (mfs_vd_priv_t *)dev->priv;
    if (p == NULL || p->bdev == NULL) {
        return MFS_DEV_EINVAL;
    }
    if (lba >= dev->block_count) {
        return MFS_DEV_RANGE;
    }

    sector = (metta_blk_lba_t)lba * (metta_blk_lba_t)MFS_SECTORS_PER_BLOCK;
    st = block_read(p->bdev, sector, buf, MFS_SECTORS_PER_BLOCK);
    return st == METTA_OK ? MFS_DEV_OK : MFS_DEV_IOERR;
}

static int mfs_vd_write(mfs_blkdev_t *dev, uint32_t lba, const void *buf)
{
    mfs_vd_priv_t *p;
    metta_blk_lba_t sector;
    metta_status_t st;

    if (dev == NULL || buf == NULL) {
        return MFS_DEV_EINVAL;
    }
    p = (mfs_vd_priv_t *)dev->priv;
    if (p == NULL || p->bdev == NULL) {
        return MFS_DEV_EINVAL;
    }
    if (lba >= dev->block_count) {
        return MFS_DEV_RANGE;
    }
    if (p->bdev->write_sectors == NULL) {
        return MFS_DEV_IOERR;
    }

    sector = (metta_blk_lba_t)lba * (metta_blk_lba_t)MFS_SECTORS_PER_BLOCK;
    st = block_write(p->bdev, sector, buf, MFS_SECTORS_PER_BLOCK);
    return st == METTA_OK ? MFS_DEV_OK : MFS_DEV_IOERR;
}

int mfs_blkdev_bind_block(mfs_blkdev_t *out, metta_block_dev_t *bdev)
{
    uint64_t blocks;

    if (out == NULL || bdev == NULL) {
        return MFS_DEV_EINVAL;
    }
    if (bdev->read_sectors == NULL) {
        return MFS_DEV_EINVAL;
    }

    blocks = bdev->sector_count / (uint64_t)MFS_SECTORS_PER_BLOCK;
    if (blocks == 0u || blocks > 0xFFFFFFFFu) {
        return MFS_DEV_EINVAL;
    }

    mfs_vd_priv.bdev = bdev;
    out->read = mfs_vd_read;
    out->write = mfs_vd_write;
    out->priv = &mfs_vd_priv;
    out->block_count = (uint32_t)blocks;
    out->block_size = MFS_BLOCK_SIZE;
    return MFS_DEV_OK;
}
#include "mfs_internal.h"

int mfs_read_super(mfs_blkdev_t *dev, mfs_superblock_t *sb)
{
    uint32_t sum;

    if (dev == NULL || sb == NULL) {
        return MFS_EINVAL;
    }
    if (mfs_blkdev_read(dev, MFS_SB_BLOCK, sb) != MFS_DEV_OK) {
        return MFS_EIO;
    }
    if (sb->magic != MFS_MAGIC || sb->version != MFS_VERSION ||
        sb->block_size != MFS_BLOCK_SIZE) {
        return MFS_ECORRUPT;
    }

    sum = mfs_super_checksum(sb, sizeof(*sb));
    if (sum != sb->checksum) {
        return MFS_ECORRUPT;
    }
    return MFS_OK;
}

int mfs_write_super(mfs_blkdev_t *dev, const mfs_superblock_t *sb)
{
    mfs_superblock_t copy;

    if (dev == NULL || sb == NULL) {
        return MFS_EINVAL;
    }

    copy = *sb;
    copy.checksum = mfs_super_checksum(&copy, sizeof(copy));
    if (mfs_blkdev_write(dev, MFS_SB_BLOCK, &copy) != MFS_DEV_OK) {
        return MFS_EIO;
    }
    return MFS_OK;
}

int mfs_sync_super(mfs_fs_t *fs)
{
    if (fs == NULL || fs->dev == NULL || !fs->mounted) {
        return MFS_EINVAL;
    }
    fs->sb.state = MFS_SUPER_STATE_CLEAN;
    return mfs_write_super(fs->dev, &fs->sb);
}

void mfs_label_write(mfs_superblock_t *sb, const char *label)
{
    unsigned i;

    for (i = 0; i < sizeof(sb->label); i++) {
        sb->label[i] = 0;
    }
    if (label == NULL) {
        return;
    }
    for (i = 0; i < sizeof(sb->label) - 1u; i++) {
        char c = label[i];
        if (c == '\0') {
            break;
        }
        sb->label[i] = (uint8_t)c;
    }
}
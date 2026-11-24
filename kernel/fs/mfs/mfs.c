#include "mfs_internal.h"

#include <stddef.h>

static int mfs_zero_block(mfs_blkdev_t *dev, uint32_t lba)
{
    uint8_t blk[MFS_BLOCK_SIZE];

    for (unsigned i = 0; i < MFS_BLOCK_SIZE; i++) {
        blk[i] = 0;
    }
    return mfs_blkdev_write(dev, lba, blk) == MFS_DEV_OK ? MFS_OK : MFS_EIO;
}

static int mfs_format_bitmap(mfs_blkdev_t *dev, uint32_t lba, uint32_t used_bits, uint32_t set_first)
{
    uint8_t blk[MFS_BLOCK_SIZE];
    unsigned i;

    for (i = 0; i < MFS_BLOCK_SIZE; i++) {
        blk[i] = 0;
    }
    for (uint32_t bit = 0; bit < set_first && bit < used_bits * 8u; bit++) {
        uint32_t byte = bit >> 3;
        uint8_t mask = (uint8_t)(1u << (bit & 7u));
        blk[byte] |= mask;
    }
    if (mfs_blkdev_write(dev, lba, blk) != MFS_DEV_OK) {
        return MFS_EIO;
    }
    return MFS_OK;
}

int mfs_format(mfs_blkdev_t *dev, const mfs_format_opts_t *opts)
{
    mfs_superblock_t sb;
    uint32_t block_count;
    uint32_t inode_count;
    uint32_t inode_tbl_blocks;
    uint32_t data_start;
    uint32_t data_blocks;
    uint32_t free_blocks;
    uint32_t i;

    if (dev == NULL) {
        return MFS_EINVAL;
    }

    block_count = opts != NULL && opts->block_count != 0u ? opts->block_count : dev->block_count;
    inode_count =
        opts != NULL && opts->inode_count != 0u ? opts->inode_count : MFS_DEFAULT_INODE_COUNT;
    inode_tbl_blocks = opts != NULL && opts->inode_table_blocks != 0u
                           ? opts->inode_table_blocks
                           : MFS_DEFAULT_INODE_TBL_BLOCKS;

    data_start = MFS_INODE_TBL_START + inode_tbl_blocks;
    if (block_count <= data_start + 4u) {
        return MFS_EINVAL;
    }
    if (inode_count > inode_tbl_blocks * MFS_INODES_PER_BLK) {
        return MFS_EINVAL;
    }

    data_blocks = block_count - data_start;
    free_blocks = data_blocks;

    for (i = 0; i < sizeof(sb); i++) {
        ((uint8_t *)&sb)[i] = 0;
    }

    sb.magic = MFS_MAGIC;
    sb.version = MFS_VERSION;
    sb.block_size = MFS_BLOCK_SIZE;
    sb.total_blocks = block_count;
    sb.inode_count = inode_count;
    sb.inode_table_start = MFS_INODE_TBL_START;
    sb.inode_table_blocks = inode_tbl_blocks;
    sb.data_start = data_start;
    sb.inode_bitmap_block = MFS_IBMAP_BLOCK;
    sb.block_bitmap_block = MFS_BMAP_BLOCK;
    sb.root_inode = 1;
    sb.free_inodes = inode_count > 2u ? inode_count - 2u : 0u;
    sb.free_blocks = free_blocks;
    sb.mount_count = 0;
    sb.state = MFS_SUPER_STATE_CLEAN;
    mfs_label_write(&sb, opts != NULL ? opts->label : "metta");

    if (mfs_write_super(dev, &sb) != MFS_OK) {
        return MFS_EIO;
    }
    if (mfs_format_bitmap(dev, MFS_IBMAP_BLOCK, inode_count, 2u) != MFS_OK) {
        return MFS_EIO;
    }
    if (mfs_format_bitmap(dev, MFS_BMAP_BLOCK, data_blocks, 0u) != MFS_OK) {
        return MFS_EIO;
    }

    for (i = MFS_INODE_TBL_START; i < data_start; i++) {
        if (mfs_zero_block(dev, i) != MFS_OK) {
            return MFS_EIO;
        }
    }

    {
        mfs_fs_t tmp;
        mfs_inode_disk_t root;

        for (i = 0; i < sizeof(tmp); i++) {
            ((uint8_t *)&tmp)[i] = 0;
        }
        tmp.dev = dev;
        tmp.sb = sb;
        tmp.mounted = 1;

        for (i = 0; i < sizeof(root); i++) {
            ((uint8_t *)&root)[i] = 0;
        }
        root.ino = 1;
        root.mode = MFS_S_IFDIR;
        root.nlink = 2;
        root.ctime = root.mtime = root.atime = 1;
        if (mfs_store_inode(&tmp, &root) != MFS_OK) {
            return MFS_EIO;
        }
        if (mfs_dir_add_entry(&tmp, &root, 1, MFS_FT_DIR, ".") != MFS_OK ||
            mfs_dir_add_entry(&tmp, &root, 1, MFS_FT_DIR, "..") != MFS_OK) {
            return MFS_EIO;
        }
        if (mfs_store_inode(&tmp, &root) != MFS_OK) {
            return MFS_EIO;
        }

        sb = tmp.sb;
        if (mfs_write_super(dev, &sb) != MFS_OK) {
            return MFS_EIO;
        }
    }

    return MFS_OK;
}

int mfs_mount(mfs_blkdev_t *dev, mfs_fs_t *fs)
{
    if (dev == NULL || fs == NULL) {
        return MFS_EINVAL;
    }

    if (mfs_read_super(dev, &fs->sb) != MFS_OK) {
        return MFS_ECORRUPT;
    }
    if (fs->sb.total_blocks > dev->block_count) {
        return MFS_ECORRUPT;
    }

    fs->dev = dev;
    fs->mounted = 1;
    fs->sb.mount_count++;
    fs->sb.state = MFS_SUPER_STATE_DIRTY;
    return mfs_write_super(dev, &fs->sb);
}

void mfs_umount(mfs_fs_t *fs)
{
    if (fs == NULL || !fs->mounted) {
        return;
    }
    (void)mfs_sync_super(fs);
    fs->mounted = 0;
}

uint32_t mfs_root_ino(const mfs_fs_t *fs)
{
    if (fs == NULL) {
        return 0;
    }
    return fs->sb.root_inode;
}
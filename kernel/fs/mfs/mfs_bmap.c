#include "mfs_internal.h"

static int mfs_bitmap_lba(const mfs_fs_t *fs, int is_inode, uint32_t *lba_out)
{
    if (fs == NULL) {
        return MFS_EINVAL;
    }
    *lba_out = is_inode ? fs->sb.inode_bitmap_block : fs->sb.block_bitmap_block;
    return MFS_OK;
}

static int mfs_bitmap_load(mfs_fs_t *fs, int is_inode, uint8_t *blk)
{
    uint32_t lba;

    if (mfs_bitmap_lba(fs, is_inode, &lba) != MFS_OK) {
        return MFS_EINVAL;
    }
    if (mfs_blkdev_read(fs->dev, lba, blk) != MFS_DEV_OK) {
        return MFS_EIO;
    }
    return MFS_OK;
}

static int mfs_bitmap_store(mfs_fs_t *fs, int is_inode, const uint8_t *blk)
{
    uint32_t lba;

    if (mfs_bitmap_lba(fs, is_inode, &lba) != MFS_OK) {
        return MFS_EINVAL;
    }
    if (mfs_blkdev_write(fs->dev, lba, blk) != MFS_DEV_OK) {
        return MFS_EIO;
    }
    return MFS_OK;
}

int mfs_bmap_test(mfs_fs_t *fs, uint32_t index, int is_inode)
{
    uint8_t blk[MFS_BLOCK_SIZE];
    uint32_t max;
    uint64_t byte;
    uint8_t bit;

    if (fs == NULL || !fs->mounted) {
        return MFS_EINVAL;
    }

    max = is_inode ? fs->sb.inode_count : (fs->sb.total_blocks - fs->sb.data_start);
    if (index >= max) {
        return MFS_EINVAL;
    }
    if (mfs_bitmap_load(fs, is_inode, blk) != MFS_OK) {
        return MFS_EIO;
    }

    byte = index >> 3;
    bit = (uint8_t)(index & 7u);
    if (byte >= MFS_BLOCK_SIZE) {
        return MFS_EINVAL;
    }
    return (blk[byte] >> bit) & 1u;
}

int mfs_bmap_set(mfs_fs_t *fs, uint32_t index, int is_inode, int used)
{
    uint8_t blk[MFS_BLOCK_SIZE];
    uint32_t max;
    uint64_t byte;
    uint8_t bit;
    uint8_t mask;

    if (fs == NULL || !fs->mounted) {
        return MFS_EINVAL;
    }

    max = is_inode ? fs->sb.inode_count : (fs->sb.total_blocks - fs->sb.data_start);
    if (index >= max) {
        return MFS_EINVAL;
    }
    if (mfs_bitmap_load(fs, is_inode, blk) != MFS_OK) {
        return MFS_EIO;
    }

    byte = index >> 3;
    bit = (uint8_t)(index & 7u);
    if (byte >= MFS_BLOCK_SIZE) {
        return MFS_EINVAL;
    }

    mask = (uint8_t)(1u << bit);
    if (used) {
        blk[byte] |= mask;
    } else {
        blk[byte] &= (uint8_t)~mask;
    }

    if (mfs_bitmap_store(fs, is_inode, blk) != MFS_OK) {
        return MFS_EIO;
    }
    return MFS_OK;
}

/* The on-disk bitmap occupies one block. Read it once per allocation,
 * including when the volume already holds a large application. */
static int allocate_bit(mfs_fs_t *fs, int inode, uint32_t *result)
{
    uint8_t map[MFS_BLOCK_SIZE];
    if (!fs || !fs->mounted || !result) return MFS_EINVAL;
    uint32_t limit = inode ? fs->sb.inode_count : fs->sb.total_blocks - fs->sb.data_start;
    if (limit > sizeof(map) * 8u) return MFS_EINVAL;
    if (!(inode ? fs->sb.free_inodes : fs->sb.free_blocks)) return MFS_ENOSPC;
    if (mfs_bitmap_load(fs, inode, map) != MFS_OK) return MFS_EIO;
    for (uint32_t i = inode ? 1u : 0u; i < limit; i++) {
        uint8_t mask = (uint8_t)(1u << (i & 7));
        if (map[i >> 3] == 255) { i |= 7u; continue; }
        if (map[i >> 3] & mask) continue;
        map[i >> 3] |= mask;
        if (mfs_bitmap_store(fs, inode, map) != MFS_OK) return MFS_EIO;
        if (inode) fs->sb.free_inodes--; else fs->sb.free_blocks--;
        *result = inode ? i : fs->sb.data_start + i;
        return MFS_OK;
    }
    return MFS_ENOSPC;
}

int mfs_alloc_inode(mfs_fs_t *fs, uint32_t *ino_out)
{
    return allocate_bit(fs, 1, ino_out);
}

int mfs_alloc_block(mfs_fs_t *fs, uint32_t *lba_out)
{
    return allocate_bit(fs, 0, lba_out);
}

int mfs_free_block(mfs_fs_t *fs, uint32_t lba)
{
    uint32_t rel;

    if (fs == NULL || !fs->mounted || lba < fs->sb.data_start) {
        return MFS_EINVAL;
    }

    rel = lba - fs->sb.data_start;
    if (mfs_bmap_set(fs, rel, 0, 0) != MFS_OK) {
        return MFS_EIO;
    }
    fs->sb.free_blocks++;
    return MFS_OK;
}

int mfs_free_inode(mfs_fs_t *fs, uint32_t ino)
{
    if (fs == NULL || !fs->mounted || ino == 0u || ino >= fs->sb.inode_count) {
        return MFS_EINVAL;
    }
    if (mfs_bmap_set(fs, ino, 1, 0) != MFS_OK) {
        return MFS_EIO;
    }
    fs->sb.free_inodes++;
    return MFS_OK;
}
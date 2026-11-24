#include "mfs_internal.h"

/* Fuera de la pila: el clic del escritorio ya lleva una pila profunda. */
static uint8_t mfs_blk_a[MFS_BLOCK_SIZE];
static uint8_t mfs_blk_b[MFS_BLOCK_SIZE];
static uint8_t mfs_blk_c[MFS_BLOCK_SIZE];

static int mfs_inode_validate(const mfs_inode_disk_t *ino)
{
    uint32_t sum;

    if (ino == NULL) {
        return MFS_EINVAL;
    }
    sum = mfs_inode_checksum(ino);
    if (sum != ino->checksum) {
        return MFS_ECORRUPT;
    }
    return MFS_OK;
}

int mfs_load_inode(mfs_fs_t *fs, uint32_t ino, mfs_inode_disk_t *out)
{
    uint32_t index;
    uint32_t lba;
    uint32_t off;
    uint8_t *blk = mfs_blk_a;

    if (fs == NULL || out == NULL || !fs->mounted) {
        return MFS_EINVAL;
    }
    if (ino >= fs->sb.inode_count) {
        return MFS_EINVAL;
    }

    index = ino;
    lba = fs->sb.inode_table_start + (index / MFS_INODES_PER_BLK);
    off = (index % MFS_INODES_PER_BLK) * MFS_INODE_SIZE;

    if (mfs_blkdev_read(fs->dev, lba, blk) != MFS_DEV_OK) {
        return MFS_EIO;
    }

    for (unsigned i = 0; i < MFS_INODE_SIZE; i++) {
        ((uint8_t *)out)[i] = blk[off + i];
    }

    return mfs_inode_validate(out);
}

int mfs_store_inode(mfs_fs_t *fs, const mfs_inode_disk_t *ino)
{
    mfs_inode_disk_t copy;
    uint32_t index;
    uint32_t lba;
    uint32_t off;
    uint8_t blk[MFS_BLOCK_SIZE];

    if (fs == NULL || ino == NULL || !fs->mounted) {
        return MFS_EINVAL;
    }
    if (ino->ino >= fs->sb.inode_count) {
        return MFS_EINVAL;
    }

    copy = *ino;
    copy.checksum = mfs_inode_checksum(&copy);

    index = ino->ino;
    lba = fs->sb.inode_table_start + (index / MFS_INODES_PER_BLK);
    off = (index % MFS_INODES_PER_BLK) * MFS_INODE_SIZE;

    if (mfs_blkdev_read(fs->dev, lba, blk) != MFS_DEV_OK) {
        return MFS_EIO;
    }

    for (unsigned i = 0; i < MFS_INODE_SIZE; i++) {
        blk[off + i] = ((const uint8_t *)&copy)[i];
    }

    if (mfs_blkdev_write(fs->dev, lba, blk) != MFS_DEV_OK) {
        return MFS_EIO;
    }
    return MFS_OK;
}

int mfs_inode_get_block(mfs_fs_t *fs, mfs_inode_disk_t *ino, uint32_t block_index,
                        int allocate, uint32_t *lba_out)
{
    uint32_t lba;
    uint32_t slot;
    uint8_t *iblk = mfs_blk_b;
    uint32_t *ptrs;

    if (fs == NULL || ino == NULL || lba_out == NULL) {
        return MFS_EINVAL;
    }

    if (block_index < MFS_DIRECT_BLOCKS) {
        lba = ino->blocks[block_index];
        if (lba == 0u && allocate) {
            if (mfs_alloc_block(fs, &lba) != MFS_OK) {
                return MFS_ENOSPC;
            }
            ino->blocks[block_index] = lba;
        }
        if (lba == 0u) {
            return MFS_ENOENT;
        }
        *lba_out = lba;
        return MFS_OK;
    }

    slot = block_index - MFS_DIRECT_BLOCKS;
    if (slot >= MFS_PTRS_PER_BLOCK) {
        uint32_t rel = slot - MFS_PTRS_PER_BLOCK;
        uint32_t page = rel / MFS_PTRS_PER_BLOCK;
        uint32_t leaf;
        uint8_t *top = mfs_blk_c;
        uint32_t *pages;

        if (page >= MFS_PTRS_PER_BLOCK) {
            return MFS_EINVAL;
        }
        if (ino->double_indirect == 0u) {
            uint32_t allocated = 0u;

            if (!allocate) {
                return MFS_ENOENT;
            }
            if (mfs_alloc_block(fs, &allocated) != MFS_OK) {
                return MFS_ENOSPC;
            }
            ino->double_indirect = allocated;
            for (unsigned i = 0; i < MFS_BLOCK_SIZE; i++) {
                top[i] = 0;
            }
            if (mfs_blkdev_write(fs->dev, ino->double_indirect, top) != MFS_DEV_OK) {
                return MFS_EIO;
            }
        }
        if (mfs_blkdev_read(fs->dev, ino->double_indirect, top) != MFS_DEV_OK) {
            return MFS_EIO;
        }
        pages = (uint32_t *)top;
        leaf = pages[page];
        if (leaf == 0u) {
            if (!allocate) {
                return MFS_ENOENT;
            }
            if (mfs_alloc_block(fs, &leaf) != MFS_OK) {
                return MFS_ENOSPC;
            }
            pages[page] = leaf;
            if (mfs_blkdev_write(fs->dev, ino->double_indirect, top) != MFS_DEV_OK) {
                return MFS_EIO;
            }
            for (unsigned i = 0; i < MFS_BLOCK_SIZE; i++) {
                iblk[i] = 0;
            }
            if (mfs_blkdev_write(fs->dev, leaf, iblk) != MFS_DEV_OK) {
                return MFS_EIO;
            }
        }
        slot = rel % MFS_PTRS_PER_BLOCK;
        if (mfs_blkdev_read(fs->dev, leaf, iblk) != MFS_DEV_OK) {
            return MFS_EIO;
        }
        ptrs = (uint32_t *)iblk;
        lba = ptrs[slot];
        if (lba == 0u && allocate) {
            if (mfs_alloc_block(fs, &lba) != MFS_OK) {
                return MFS_ENOSPC;
            }
            ptrs[slot] = lba;
            if (mfs_blkdev_write(fs->dev, leaf, iblk) != MFS_DEV_OK) {
                return MFS_EIO;
            }
        }
        if (lba == 0u) {
            return MFS_ENOENT;
        }
        *lba_out = lba;
        return MFS_OK;
    }

    if (ino->indirect == 0u) {
        uint32_t allocated = 0u;

        if (!allocate) {
            return MFS_ENOENT;
        }
        if (mfs_alloc_block(fs, &allocated) != MFS_OK) {
            return MFS_ENOSPC;
        }
        ino->indirect = allocated;
        for (unsigned i = 0; i < MFS_BLOCK_SIZE; i++) {
            iblk[i] = 0;
        }
        if (mfs_blkdev_write(fs->dev, ino->indirect, iblk) != MFS_DEV_OK) {
            return MFS_EIO;
        }
    }

    if (mfs_blkdev_read(fs->dev, ino->indirect, iblk) != MFS_DEV_OK) {
        return MFS_EIO;
    }
    ptrs = (uint32_t *)iblk;
    lba = ptrs[slot];
    if (lba == 0u && allocate) {
        if (mfs_alloc_block(fs, &lba) != MFS_OK) {
            return MFS_ENOSPC;
        }
        ptrs[slot] = lba;
        if (mfs_blkdev_write(fs->dev, ino->indirect, iblk) != MFS_DEV_OK) {
            return MFS_EIO;
        }
    }
    if (lba == 0u) {
        return MFS_ENOENT;
    }
    *lba_out = lba;
    return MFS_OK;
}

int mfs_inode_truncate_blocks(mfs_fs_t *fs, mfs_inode_disk_t *ino, uint32_t new_block_count)
{
    uint32_t old_blocks = (ino->size + MFS_BLOCK_SIZE - 1u) / MFS_BLOCK_SIZE;
    uint32_t i;
    uint32_t lba;

    (void)new_block_count;

    for (i = 0; i < old_blocks; i++) {
        if (mfs_inode_get_block(fs, ino, i, 0, &lba) == MFS_OK) {
            if (i < MFS_DIRECT_BLOCKS) {
                ino->blocks[i] = 0u;
            }
            mfs_free_block(fs, lba);
        }
    }

    if (ino->indirect != 0u) {
        mfs_free_block(fs, ino->indirect);
        ino->indirect = 0u;
    }
    if (ino->double_indirect != 0u) {
        uint8_t top[MFS_BLOCK_SIZE];
        uint32_t *pages;
        uint32_t p;

        if (mfs_blkdev_read(fs->dev, ino->double_indirect, top) == MFS_DEV_OK) {
            pages = (uint32_t *)top;
            for (p = 0; p < MFS_PTRS_PER_BLOCK; p++) {
                if (pages[p] != 0u) {
                    mfs_free_block(fs, pages[p]);
                }
            }
        }
        mfs_free_block(fs, ino->double_indirect);
        ino->double_indirect = 0u;
    }
    ino->size = 0u;
    return MFS_OK;
}

int mfs_read_inode(mfs_fs_t *fs, uint32_t ino, mfs_inode_disk_t *out)
{
    return mfs_load_inode(fs, ino, out);
}

int mfs_read_file(mfs_fs_t *fs, uint32_t ino, uint64_t offset, void *buf, size_t len,
                  size_t *out_read)
{
    mfs_inode_disk_t id;
    uint8_t *blk = mfs_blk_a;
    uint8_t *dst;
    size_t done = 0;
    uint64_t file_size;
    uint32_t block_index;
    uint32_t block_off;
    uint32_t lba;
    size_t chunk;

    if (fs == NULL || buf == NULL || out_read == NULL) {
        return MFS_EINVAL;
    }
    if (mfs_load_inode(fs, ino, &id) != MFS_OK) {
        return MFS_EIO;
    }

    file_size = id.size;
    if (offset >= file_size) {
        *out_read = 0;
        return MFS_OK;
    }
    if (offset + len > file_size) {
        len = (size_t)(file_size - offset);
    }

    dst = (uint8_t *)buf;
    while (done < len) {
        block_index = (uint32_t)((offset + done) / MFS_BLOCK_SIZE);
        block_off = (uint32_t)((offset + done) % MFS_BLOCK_SIZE);
        if (mfs_inode_get_block(fs, &id, block_index, 0, &lba) != MFS_OK) {
            break;
        }
        if (mfs_blkdev_read(fs->dev, lba, blk) != MFS_DEV_OK) {
            return MFS_EIO;
        }
        chunk = MFS_BLOCK_SIZE - block_off;
        if (chunk > len - done) {
            chunk = len - done;
        }
        for (size_t i = 0; i < chunk; i++) {
            dst[done + i] = blk[block_off + i];
        }
        done += chunk;
    }

    *out_read = done;
    if (done < len) {
        return MFS_EIO;
    }
    return MFS_OK;
}

int mfs_write_file(mfs_fs_t *fs, uint32_t ino, uint64_t offset, const void *buf,
                   size_t len, size_t *out_written)
{
    mfs_inode_disk_t id;
    uint8_t blk[MFS_BLOCK_SIZE];
    const uint8_t *src;
    size_t done = 0;
    uint32_t block_index;
    uint32_t block_off;
    uint32_t lba;
    size_t chunk;

    if (fs == NULL || buf == NULL || out_written == NULL) {
        return MFS_EINVAL;
    }
    if (mfs_load_inode(fs, ino, &id) != MFS_OK) {
        return MFS_EIO;
    }

    src = (const uint8_t *)buf;
    while (done < len) {
        block_index = (uint32_t)((offset + done) / MFS_BLOCK_SIZE);
        block_off = (uint32_t)((offset + done) % MFS_BLOCK_SIZE);

        if (mfs_inode_get_block(fs, &id, block_index, 1, &lba) != MFS_OK) {
            return MFS_ENOSPC;
        }

        chunk = MFS_BLOCK_SIZE - block_off;
        if (chunk > len - done) {
            chunk = len - done;
        }

        if (block_off != 0u || chunk < MFS_BLOCK_SIZE) {
            if (mfs_blkdev_read(fs->dev, lba, blk) != MFS_DEV_OK) {
                return MFS_EIO;
            }
        } else {
            for (unsigned i = 0; i < MFS_BLOCK_SIZE; i++) {
                blk[i] = 0;
            }
        }

        for (size_t i = 0; i < chunk; i++) {
            blk[block_off + i] = src[done + i];
        }
        if (mfs_blkdev_write(fs->dev, lba, blk) != MFS_DEV_OK) {
            return MFS_EIO;
        }
        done += chunk;
    }

    if (offset + done > id.size) {
        id.size = (uint32_t)(offset + done);
    }
    id.mtime++;
    if (mfs_store_inode(fs, &id) != MFS_OK) {
        return MFS_EIO;
    }

    *out_written = done;
    return MFS_OK;
}
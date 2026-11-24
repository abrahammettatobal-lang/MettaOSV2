#include "mfs_internal.h"

static int mfs_name_eq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static uint16_t mfs_dirent_rec_len(uint8_t name_len)
{
    uint16_t need = (uint16_t)(MFS_DIRENT_HDR_SIZE + name_len);
    uint16_t align4 = (uint16_t)((need + 3u) & ~3u);
    return align4;
}

void mfs_name_copy(mfs_dirent_info_t *ent, const char *name, uint8_t len)
{
    unsigned i;

    if (ent == NULL) {
        return;
    }
    if (len > MFS_NAME_MAX) {
        len = MFS_NAME_MAX;
    }
    for (i = 0; i < len; i++) {
        ent->name[i] = name[i];
    }
    ent->name[len] = '\0';
}

int mfs_dir_add_entry(mfs_fs_t *fs, mfs_inode_disk_t *dir, uint32_t child_ino,
                      uint8_t file_type, const char *name)
{
    uint8_t name_len;
    uint16_t rec_len;
    uint32_t block_index;
    uint32_t lba;
    uint8_t blk[MFS_BLOCK_SIZE];
    uint32_t pos;

    if (fs == NULL || dir == NULL || name == NULL) {
        return MFS_EINVAL;
    }
    if (!mfs_inode_mode_is_dir(dir->mode)) {
        return MFS_ENOTDIR;
    }

    for (name_len = 0; name[name_len] != '\0' && name_len < MFS_NAME_MAX; name_len++) {
    }
    if (name_len == 0u) {
        return MFS_EINVAL;
    }

    rec_len = mfs_dirent_rec_len(name_len);

    for (block_index = 0; block_index < MFS_DIRECT_BLOCKS; block_index++) {
        if (dir->blocks[block_index] == 0u) {
            if (mfs_alloc_block(fs, &lba) != MFS_OK) {
                return MFS_ENOSPC;
            }
            dir->blocks[block_index] = lba;
            for (unsigned i = 0; i < MFS_BLOCK_SIZE; i++) {
                blk[i] = 0;
            }
        } else {
            lba = dir->blocks[block_index];
            if (mfs_blkdev_read(fs->dev, lba, blk) != MFS_DEV_OK) {
                return MFS_EIO;
            }
        }

        for (pos = 0; pos + MFS_DIRENT_HDR_SIZE <= MFS_BLOCK_SIZE; ) {
            mfs_dirent_hdr_t *hdr = (mfs_dirent_hdr_t *)(blk + pos);
            if (hdr->rec_len == 0u) {
                if (pos + rec_len > MFS_BLOCK_SIZE) {
                    break;
                }
                hdr->ino = child_ino;
                hdr->name_len = name_len;
                hdr->file_type = file_type;
                hdr->rec_len = rec_len;
                for (unsigned i = 0; i < name_len; i++) {
                    blk[pos + MFS_DIRENT_HDR_SIZE + i] = (uint8_t)name[i];
                }
                if (mfs_blkdev_write(fs->dev, lba, blk) != MFS_DEV_OK) {
                    return MFS_EIO;
                }
                dir->size += rec_len;
                return MFS_OK;
            }
            if (hdr->ino == 0u) {
                if (pos + rec_len > MFS_BLOCK_SIZE) {
                    break;
                }
                hdr->ino = child_ino;
                hdr->name_len = name_len;
                hdr->file_type = file_type;
                hdr->rec_len = rec_len;
                for (unsigned i = 0; i < name_len; i++) {
                    blk[pos + MFS_DIRENT_HDR_SIZE + i] = (uint8_t)name[i];
                }
                if (mfs_blkdev_write(fs->dev, lba, blk) != MFS_DEV_OK) {
                    return MFS_EIO;
                }
                dir->size += rec_len;
                return MFS_OK;
            }
            pos += hdr->rec_len;
        }
    }
    return MFS_ENOSPC;
}

int mfs_lookup(mfs_fs_t *fs, uint32_t dir_ino, const char *name, uint32_t *out_ino)
{
    mfs_inode_disk_t dir;
    uint32_t block_index;
    uint32_t pos;
    uint8_t blk[MFS_BLOCK_SIZE];

    if (fs == NULL || name == NULL || out_ino == NULL) {
        return MFS_EINVAL;
    }
    if (mfs_load_inode(fs, dir_ino, &dir) != MFS_OK) {
        return MFS_EIO;
    }
    if (!mfs_inode_mode_is_dir(dir.mode)) {
        return MFS_ENOTDIR;
    }

    for (block_index = 0; block_index < MFS_DIRECT_BLOCKS; block_index++) {
        if (dir.blocks[block_index] == 0u) {
            continue;
        }
        if (mfs_blkdev_read(fs->dev, dir.blocks[block_index], blk) != MFS_DEV_OK) {
            return MFS_EIO;
        }
        for (pos = 0; pos + MFS_DIRENT_HDR_SIZE <= MFS_BLOCK_SIZE; ) {
            mfs_dirent_hdr_t *hdr = (mfs_dirent_hdr_t *)(blk + pos);
            if (hdr->rec_len == 0u) {
                break;
            }
            if (hdr->ino != 0u && hdr->name_len > 0u) {
                char tmp[MFS_NAME_MAX + 1u];
                unsigned i;
                for (i = 0; i < hdr->name_len && i < MFS_NAME_MAX; i++) {
                    tmp[i] = (char)blk[pos + MFS_DIRENT_HDR_SIZE + i];
                }
                tmp[i] = '\0';
                if (mfs_name_eq(tmp, name)) {
                    *out_ino = hdr->ino;
                    return MFS_OK;
                }
            }
            pos += hdr->rec_len;
        }
    }
    return MFS_ENOENT;
}

int mfs_readdir_init(mfs_readdir_ctx_t *ctx)
{
    if (ctx == NULL) {
        return MFS_EINVAL;
    }
    ctx->block_lba = 0;
    ctx->offset = 0;
    return MFS_OK;
}

int mfs_readdir(mfs_fs_t *fs, uint32_t dir_ino, mfs_readdir_ctx_t *ctx,
                mfs_dirent_info_t *out)
{
    mfs_inode_disk_t dir;
    uint8_t blk[MFS_BLOCK_SIZE];
    uint32_t block_index;
    mfs_dirent_hdr_t *hdr;

    if (fs == NULL || ctx == NULL || out == NULL) {
        return MFS_EINVAL;
    }
    if (mfs_load_inode(fs, dir_ino, &dir) != MFS_OK) {
        return MFS_EIO;
    }
    if (!mfs_inode_mode_is_dir(dir.mode)) {
        return MFS_ENOTDIR;
    }

    for (block_index = ctx->block_lba; block_index < MFS_DIRECT_BLOCKS; block_index++) {
        if (dir.blocks[block_index] == 0u) {
            ctx->block_lba = block_index + 1u;
            ctx->offset = 0;
            continue;
        }
        if (block_index != ctx->block_lba) {
            ctx->offset = 0;
        }
        if (mfs_blkdev_read(fs->dev, dir.blocks[block_index], blk) != MFS_DEV_OK) {
            return MFS_EIO;
        }

        while (ctx->offset + MFS_DIRENT_HDR_SIZE <= MFS_BLOCK_SIZE) {
            hdr = (mfs_dirent_hdr_t *)(blk + ctx->offset);
            if (hdr->rec_len == 0u) {
                break;
            }
            if (hdr->ino != 0u) {
                out->ino = hdr->ino;
                out->file_type = hdr->file_type;
                mfs_name_copy(out, (const char *)(blk + ctx->offset + MFS_DIRENT_HDR_SIZE),
                              hdr->name_len);
                ctx->offset += hdr->rec_len;
                ctx->block_lba = block_index;
                return MFS_OK;
            }
            ctx->offset += hdr->rec_len;
        }
        ctx->block_lba = block_index + 1u;
        ctx->offset = 0;
    }
    return MFS_ENOENT;
}

static int mfs_init_new_inode(mfs_fs_t *fs, uint32_t ino, uint16_t mode, mfs_inode_disk_t *id)
{
    unsigned i;

    if (id == NULL) {
        return MFS_EINVAL;
    }
    for (i = 0; i < sizeof(*id); i++) {
        ((uint8_t *)id)[i] = 0;
    }
    id->ino = ino;
    id->mode = mode;
    id->nlink = 1;
    id->atime = 1;
    id->mtime = 1;
    id->ctime = 1;
    return mfs_store_inode(fs, id);
}

int mfs_mkdir(mfs_fs_t *fs, uint32_t parent_ino, const char *name, uint32_t *out_ino)
{
    mfs_inode_disk_t parent;
    mfs_inode_disk_t child;
    uint32_t ino;
    int st;

    if (fs == NULL || name == NULL) {
        return MFS_EINVAL;
    }
    if (mfs_lookup(fs, parent_ino, name, &ino) == MFS_OK) {
        return MFS_EEXIST;
    }
    if (mfs_load_inode(fs, parent_ino, &parent) != MFS_OK) {
        return MFS_EIO;
    }
    if (mfs_alloc_inode(fs, &ino) != MFS_OK) {
        return MFS_ENOSPC;
    }
    if (mfs_init_new_inode(fs, ino, MFS_S_IFDIR, &child) != MFS_OK) {
        return MFS_EIO;
    }
    st = mfs_dir_add_entry(fs, &parent, ino, MFS_FT_DIR, name);
    if (st != MFS_OK) {
        return st;
    }
    if (mfs_store_inode(fs, &parent) != MFS_OK) {
        return MFS_EIO;
    }
    if (mfs_dir_add_entry(fs, &child, ino, MFS_FT_DIR, ".") != MFS_OK ||
        mfs_dir_add_entry(fs, &child, parent_ino, MFS_FT_DIR, "..") != MFS_OK) {
        return MFS_EIO;
    }
    if (mfs_store_inode(fs, &child) != MFS_OK) {
        return MFS_EIO;
    }
    if (out_ino != NULL) {
        *out_ino = ino;
    }
    return MFS_OK;
}

int mfs_create_file(mfs_fs_t *fs, uint32_t parent_ino, const char *name, uint32_t *out_ino)
{
    mfs_inode_disk_t parent;
    mfs_inode_disk_t child;
    uint32_t ino;
    int st;

    if (fs == NULL || name == NULL) {
        return MFS_EINVAL;
    }
    if (mfs_lookup(fs, parent_ino, name, &ino) == MFS_OK) {
        return MFS_EEXIST;
    }
    if (mfs_load_inode(fs, parent_ino, &parent) != MFS_OK) {
        return MFS_EIO;
    }
    if (mfs_alloc_inode(fs, &ino) != MFS_OK) {
        return MFS_ENOSPC;
    }
    if (mfs_init_new_inode(fs, ino, MFS_S_IFREG, &child) != MFS_OK) {
        return MFS_EIO;
    }
    st = mfs_dir_add_entry(fs, &parent, ino, MFS_FT_REG, name);
    if (st != MFS_OK) {
        return st;
    }
    if (mfs_store_inode(fs, &parent) != MFS_OK) {
        return MFS_EIO;
    }
    if (out_ino != NULL) {
        *out_ino = ino;
    }
    return MFS_OK;
}
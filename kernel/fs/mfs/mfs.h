#ifndef MFS_MFS_H
#define MFS_MFS_H

#include <stddef.h>
#include <stdint.h>

#include "blkdev.h"
#include "mfs_format.h"

#define MFS_OK           0
#define MFS_EINVAL      -1
#define MFS_ENOENT      -2
#define MFS_EEXIST      -3
#define MFS_ENOSPC      -4
#define MFS_EIO         -5
#define MFS_ECORRUPT    -6
#define MFS_ENOTDIR     -7
#define MFS_ENOTEMPTY   -8
#define MFS_ENOMEM      -9

typedef struct {
    uint32_t block_count;
    uint32_t inode_count;
    uint32_t inode_table_blocks;
    const char *label;
} mfs_format_opts_t;

typedef struct {
    mfs_superblock_t sb;
    mfs_blkdev_t    *dev;
    int              mounted;
} mfs_fs_t;

typedef struct {
    uint32_t block_lba;
    uint32_t offset;
} mfs_readdir_ctx_t;

typedef struct {
    uint32_t ino;
    uint8_t  file_type;
    char     name[MFS_NAME_MAX + 1u];
} mfs_dirent_info_t;

int mfs_format(mfs_blkdev_t *dev, const mfs_format_opts_t *opts);
int mfs_mount(mfs_blkdev_t *dev, mfs_fs_t *fs);
void mfs_umount(mfs_fs_t *fs);

int mfs_sync_super(mfs_fs_t *fs);

uint32_t mfs_root_ino(const mfs_fs_t *fs);

int mfs_lookup(mfs_fs_t *fs, uint32_t dir_ino, const char *name, uint32_t *out_ino);
int mfs_mkdir(mfs_fs_t *fs, uint32_t parent_ino, const char *name, uint32_t *out_ino);
int mfs_create_file(mfs_fs_t *fs, uint32_t parent_ino, const char *name, uint32_t *out_ino);

int mfs_readdir_init(mfs_readdir_ctx_t *ctx);
int mfs_readdir(mfs_fs_t *fs, uint32_t dir_ino, mfs_readdir_ctx_t *ctx,
                mfs_dirent_info_t *out);

int mfs_read_inode(mfs_fs_t *fs, uint32_t ino, mfs_inode_disk_t *out);
int mfs_read_file(mfs_fs_t *fs, uint32_t ino, uint64_t offset, void *buf, size_t len,
                  size_t *out_read);
int mfs_write_file(mfs_fs_t *fs, uint32_t ino, uint64_t offset, const void *buf,
                   size_t len, size_t *out_written);

#endif /* MFS_MFS_H */
#ifndef MFS_INTERNAL_H
#define MFS_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "mfs.h"
#include "mfs_crc.h"

static inline int mfs_inode_mode_is_dir(uint16_t mode)
{
    return (mode & 0xF000u) == MFS_S_IFDIR;
}

int mfs_read_super(mfs_blkdev_t *dev, mfs_superblock_t *sb);
int mfs_write_super(mfs_blkdev_t *dev, const mfs_superblock_t *sb);

int mfs_bmap_test(mfs_fs_t *fs, uint32_t lba, int is_inode);
int mfs_bmap_set(mfs_fs_t *fs, uint32_t index, int is_inode, int used);
int mfs_alloc_inode(mfs_fs_t *fs, uint32_t *ino_out);
int mfs_alloc_block(mfs_fs_t *fs, uint32_t *lba_out);
int mfs_free_block(mfs_fs_t *fs, uint32_t lba);
int mfs_free_inode(mfs_fs_t *fs, uint32_t ino);

int mfs_load_inode(mfs_fs_t *fs, uint32_t ino, mfs_inode_disk_t *out);
int mfs_store_inode(mfs_fs_t *fs, const mfs_inode_disk_t *ino);

int mfs_inode_get_block(mfs_fs_t *fs, mfs_inode_disk_t *ino, uint32_t block_index,
                        int allocate, uint32_t *lba_out);
int mfs_inode_truncate_blocks(mfs_fs_t *fs, mfs_inode_disk_t *ino, uint32_t new_block_count);

void mfs_name_copy(mfs_dirent_info_t *ent, const char *name, uint8_t len);

int mfs_dir_add_entry(mfs_fs_t *fs, mfs_inode_disk_t *dir, uint32_t child_ino,
                      uint8_t file_type, const char *name);
void mfs_label_write(mfs_superblock_t *sb, const char *label);

#endif /* MFS_INTERNAL_H */
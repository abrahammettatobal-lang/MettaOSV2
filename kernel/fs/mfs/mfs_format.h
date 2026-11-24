#ifndef MFS_FORMAT_H
#define MFS_FORMAT_H

#include <stdint.h>

/*
 * Metta File System (MFS) — diseño on-disk v1.
 * Bloque nativo: 4096 bytes (METTA_PAGE_SIZE).
 */

#define MFS_MAGIC           0x4D465331u /* "MFS1" */
#define MFS_VERSION         1u
#define MFS_BLOCK_SIZE      4096u

#define MFS_NAME_MAX        255u
#define MFS_PATH_MAX        1024u

#define MFS_INODE_SIZE      128u
#define MFS_INODES_PER_BLK  (MFS_BLOCK_SIZE / MFS_INODE_SIZE)

#define MFS_DIRECT_BLOCKS   12u
#define MFS_PTRS_PER_BLOCK  (MFS_BLOCK_SIZE / sizeof(uint32_t))

#define MFS_SB_BLOCK        0u
#define MFS_IBMAP_BLOCK     1u
#define MFS_BMAP_BLOCK      2u
#define MFS_INODE_TBL_START 3u

#define MFS_FT_UNKNOWN      0u
#define MFS_FT_REG          1u
#define MFS_FT_DIR          2u
#define MFS_FT_SYMLINK      3u

#define MFS_S_IFREG         0x8000u
#define MFS_S_IFDIR         0x4000u
#define MFS_S_IFLNK         0xA000u

#define MFS_SUPER_STATE_CLEAN  0u
#define MFS_SUPER_STATE_DIRTY  1u

/* Valores por defecto del stub de volumen (4 MiB). */
#define MFS_STUB_BLOCK_COUNT    1024u
#define MFS_DEFAULT_INODE_COUNT 256u
#define MFS_DEFAULT_INODE_TBL_BLOCKS 8u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_count;
    uint32_t inode_table_start;
    uint32_t inode_table_blocks;
    uint32_t data_start;
    uint32_t inode_bitmap_block;
    uint32_t block_bitmap_block;
    uint32_t root_inode;
    uint32_t free_inodes;
    uint32_t free_blocks;
    uint64_t mount_count;
    uint32_t state;
    uint32_t checksum;
    uint8_t  label[32];
    uint8_t  reserved[3996];
} mfs_superblock_t;

_Static_assert(sizeof(mfs_superblock_t) == MFS_BLOCK_SIZE, "superblock must fill one block");

typedef struct __attribute__((packed)) {
    uint32_t ino;
    uint16_t mode;
    uint16_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t blocks[MFS_DIRECT_BLOCKS];
    uint32_t indirect;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    uint32_t checksum;
    /* Un nivel más: 1024 bloques de punteros. 0 si el archivo cabe en el indirecto simple. */
    uint32_t double_indirect;
    uint8_t  reserved[36];
} mfs_inode_disk_t;

_Static_assert(sizeof(mfs_inode_disk_t) == MFS_INODE_SIZE, "inode disk size");

typedef struct __attribute__((packed)) {
    uint32_t ino;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    /* name[name_len] sigue en el bloque; rec_len >= 8 + name_len alineado a 4 */
} mfs_dirent_hdr_t;

#define MFS_DIRENT_HDR_SIZE  (sizeof(mfs_dirent_hdr_t))

#endif /* MFS_FORMAT_H */
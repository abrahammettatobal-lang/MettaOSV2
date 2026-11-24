#include <stdio.h>
#include <stdint.h>

#define MFS_BLOCK_SIZE 4096u
#define MFS_INODE_SIZE 128u
#define MFS_DIRECT_BLOCKS 12u

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
} sb_t;

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
    uint8_t  reserved[40];
} in_t;

int main(void)
{
    printf("sb=%zu want=%u pad=%zu\n", sizeof(sb_t), MFS_BLOCK_SIZE,
           sizeof(sb_t) - 100u);
    printf("in=%zu want=%u\n", sizeof(in_t), MFS_INODE_SIZE);
    return 0;
}

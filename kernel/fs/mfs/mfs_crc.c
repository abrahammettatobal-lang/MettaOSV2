#include "mfs_crc.h"

#include <stddef.h>

static uint32_t mfs_crc32_byte(uint32_t crc, uint8_t byte)
{
    crc ^= (uint32_t)byte;
    for (unsigned i = 0; i < 8u; i++) {
        if ((crc & 1u) != 0u) {
            crc = (crc >> 1) ^ 0xEDB88320u;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

uint32_t mfs_crc32(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < len; i++) {
        crc = mfs_crc32_byte(crc, p[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t mfs_super_checksum(const void *sb, size_t len)
{
    const uint8_t *raw = (const uint8_t *)sb;
    size_t off = offsetof(mfs_superblock_t, checksum);
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < off; i++) {
        crc = mfs_crc32_byte(crc, raw[i]);
    }
    for (size_t i = off + sizeof(uint32_t); i < len; i++) {
        crc = mfs_crc32_byte(crc, raw[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t mfs_inode_checksum(const mfs_inode_disk_t *ino)
{
    const uint8_t *raw = (const uint8_t *)ino;
    size_t off = offsetof(mfs_inode_disk_t, checksum);
    size_t len = sizeof(*ino);
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < off; i++) {
        crc = mfs_crc32_byte(crc, raw[i]);
    }
    for (size_t i = off + sizeof(uint32_t); i < len; i++) {
        crc = mfs_crc32_byte(crc, raw[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}
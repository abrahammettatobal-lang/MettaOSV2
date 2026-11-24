#ifndef MFS_CRC_H
#define MFS_CRC_H

#include <stddef.h>
#include <stdint.h>

#include "mfs_format.h"

uint32_t mfs_crc32(const void *data, size_t len);
uint32_t mfs_super_checksum(const void *sb, size_t len);
uint32_t mfs_inode_checksum(const mfs_inode_disk_t *ino);

#endif /* MFS_CRC_H */
/*
 * Inflador zlib (RFC 1950 + RFC 1951) usado por los bloques SquashFS
 * con compresor id 1. No es reentrante: las tablas Huffman son estáticas.
 */
#ifndef METTA_FS_SQUASHFS_ZLIB_H
#define METTA_FS_SQUASHFS_ZLIB_H

#include <stdint.h>

#include "fs/squashfs.h"

int squashfs_zlib_decompress(const void *src, uint64_t src_len,
                             void *dst, uint64_t dst_cap, uint64_t *out_len);

#endif /* METTA_FS_SQUASHFS_ZLIB_H */

/*
 * MettaOS — SquashFS 4.0 little-endian (payload de AppImage tipo 2).
 *
 * Un AppImage tipo 2 es un ELF (el runtime) y, a continuación, la imagen
 * SquashFS. La magia "hsqs" del runtime no es el superbloque: el runtime
 * la trae embebida para buscarla. El offset real es el final del ELF, el
 * mismo criterio que el runtime oficial (máximo entre el final de la tabla
 * de secciones, el contenido de cada sección que no sea NOBITS y el
 * p_offset+p_filesz de cada program header). En el Firefox beta ese punto
 * coincide con el superbloque, sin relleno. Si el empaquetador alinea el
 * payload, se aceptan hasta 64 KiB por delante de ese final.
 * Los bytes 'A','I',0x02 en e_ident son una marca del runtime, no el offset.
 *
 * El superbloque en disco es el de Linux 4.0 (96 bytes): magic, inodes,
 * mkfs_time, block_size, fragments, compression, block_log, flags, no_ids,
 * s_major, s_minor, root_inode, bytes_used y las seis tablas. No se monta
 * en el VFS y nadie llama esto desde kmain.
 *
 * Compresor implementado: zlib (id 1). xz/lzma/lzo/lz4/zstd se detectan
 * y list_root devuelve SQUASHFS_ERR_COMP.
 *
 * squashfs_read_file resuelve un camino desde la raíz, lee el inodo
 * regular (tipo 2 o 9) y descomprime sus bloques de datos. En esa lista
 * el bit de “sin comprimir” es el 24; el 15 es solo de los metadatos.
 * Si el resto del archivo vive en un fragmento, se lee la tabla de
 * fragmentos. Esos bytes están en el cuerpo de la imagen: el llamador
 * tiene que pasar el AppImage entero, no solo el ELF del runtime.
 */
#include "fs/squashfs.h"
#include "fs/squashfs_zlib.h"

#include <metta/types.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SQUASHFS_MAGIC            0x73717368u /* "hsqs" */
#define SQUASHFS_MAGIC_SWAP       0x68737173u /* "sqsh" */

#define SQUASHFS_METADATA_SIZE    8192u
#define SQUASHFS_NAME_MAX         256u
#define SQUASHFS_DIR_HEADER       12u
#define SQUASHFS_DIR_ENTRY        8u
#define SQUASHFS_DIR_COUNT_MAX    256u
#define SQUASHFS_ROOT_DIR_MAX     (256u * 1024u)
#define SQUASHFS_SCAN_LIMIT       (1024u * 1024u)
#define SQUASHFS_ELF_PAD          (64u * 1024u)

#define SQUASHFS_DIR_TYPE         1u
#define SQUASHFS_REG_TYPE         2u
#define SQUASHFS_SYMLINK_TYPE     3u
#define SQUASHFS_LDIR_TYPE        8u
#define SQUASHFS_LREG_TYPE        9u
#define SQUASHFS_LSYMLINK_TYPE    10u
#define SQUASHFS_INVALID_BLK      (~(metta_u64)0)
#define SQUASHFS_INVALID_FRAG     0xffffffffu
#define SQUASHFS_COMP_BIT_BLOCK   (1u << 24)
#define SQUASHFS_DATA_MAX         (128u * 1024u)
#define SQUASHFS_LINK_MAX         512u
#define SQUASHFS_LINK_HOPS        4u

typedef metta_status_t (*metta_io_read_fn)(void *ctx, metta_u64 offset,
                                           void *buf, metta_u64 len);

typedef struct metta_io_blob {
    void *ctx;
    metta_io_read_fn read;
    metta_u64 size;
} metta_io_blob_t;

/* Mismo orden que kernel/linux/appimage.c (no hay cabecera compartida). */
typedef struct squashfs_image_info {
    metta_u64 offset;
    metta_u16 version_major;
    metta_u16 version_minor;
    metta_u32 block_size;
    metta_u32 compression;
    int       big_endian;
} squashfs_image_info_t;

typedef struct squashfs_sb {
    metta_u32 inodes;
    metta_u32 block_size;
    metta_u16 compression;
    metta_u16 block_log;
    metta_u16 version_major;
    metta_u16 version_minor;
    metta_u64 root_inode;
    metta_u64 bytes_used;
    metta_u64 inode_table_start;
    metta_u64 directory_table_start;
    metta_u64 fragment_table_start;
    metta_u32 fragments;
} squashfs_sb_t;

typedef struct sq_meta {
    const metta_u8 *image;
    metta_u64       size;
    metta_u64       block;
    metta_u32       offset;
    metta_u32       unc_len;
    metta_u64       next_block;
    int             loaded;
    metta_u64       loaded_block;
} sq_meta_t;

/* Un bloque de metadatos descomprimido. No reentrante. */
static metta_u8 g_meta_unc[SQUASHFS_METADATA_SIZE];
/* Un bloque de datos (mksquashfs usa 128 KiB en este AppImage). */
static metta_u8 g_data_unc[SQUASHFS_DATA_MAX];

static metta_u16 rd16(const metta_u8 *p)
{
    return (metta_u16)p[0] | (metta_u16)((metta_u16)p[1] << 8);
}

static metta_u32 rd32(const metta_u8 *p)
{
    return (metta_u32)p[0] |
           ((metta_u32)p[1] << 8) |
           ((metta_u32)p[2] << 16) |
           ((metta_u32)p[3] << 24);
}

static metta_u64 rd64(const metta_u8 *p)
{
    return (metta_u64)rd32(p) | ((metta_u64)rd32(p + 4) << 32);
}

static int fits(metta_u64 size, metta_u64 off, metta_u64 len)
{
    if (len > size) {
        return 0;
    }
    if (off > size - len) {
        return 0;
    }
    return 1;
}

static int table_in_image(metta_u64 off, metta_u64 bytes_used)
{
    if (off == SQUASHFS_INVALID_BLK) {
        return 1;
    }
    return off >= 96u && off < bytes_used;
}

/*
 * Superbloque 4.0 little-endian. SQUASHFS_ERR_NOENT si la magia no es hsqs.
 * SQUASHFS_ERR_CORRUPT si la magia coincide pero el resto no es 4.0 usable.
 */
static int sb_parse(const metta_u8 *b, metta_u64 avail, squashfs_sb_t *out)
{
    squashfs_sb_t sb;
    metta_u32 magic;
    metta_u32 root_off;
    metta_u64 root_blk;
    metta_u64 id_table;
    metta_u64 xattr_table;
    metta_u64 frag_table;
    metta_u64 lookup_table;
    metta_u64 inode_span;

    if (out == NULL) {
        return SQUASHFS_ERR_INVAL;
    }
    if (avail < 96u) {
        return SQUASHFS_ERR_RANGE;
    }

    magic = rd32(b);
    if (magic == SQUASHFS_MAGIC_SWAP) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (magic != SQUASHFS_MAGIC) {
        return SQUASHFS_ERR_NOENT;
    }

    sb.inodes = rd32(b + 4);
    sb.block_size = rd32(b + 12);
    sb.compression = rd16(b + 20);
    sb.block_log = rd16(b + 22);
    sb.version_major = rd16(b + 28);
    sb.version_minor = rd16(b + 30);
    sb.root_inode = rd64(b + 32);
    sb.bytes_used = rd64(b + 40);
    id_table = rd64(b + 48);
    xattr_table = rd64(b + 56);
    sb.fragments = rd32(b + 16);
    sb.inode_table_start = rd64(b + 64);
    sb.directory_table_start = rd64(b + 72);
    frag_table = rd64(b + 80);
    sb.fragment_table_start = frag_table;
    lookup_table = rd64(b + 88);

    if (sb.version_major != 4u || sb.version_minor != 0u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (sb.block_log < 12u || sb.block_log > 20u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (sb.block_size != (1u << sb.block_log)) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (sb.compression < SQUASHFS_COMP_ZLIB || sb.compression > SQUASHFS_COMP_ZSTD) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (sb.inodes == 0u || sb.bytes_used < 96u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (sb.inode_table_start < 96u || sb.inode_table_start >= sb.bytes_used) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (sb.directory_table_start < 96u || sb.directory_table_start >= sb.bytes_used) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (!table_in_image(id_table, sb.bytes_used) || id_table == SQUASHFS_INVALID_BLK) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (!table_in_image(xattr_table, sb.bytes_used) ||
        !table_in_image(frag_table, sb.bytes_used) ||
        !table_in_image(lookup_table, sb.bytes_used)) {
        return SQUASHFS_ERR_CORRUPT;
    }

    root_off = (metta_u32)(sb.root_inode & 0xffffu);
    root_blk = sb.root_inode >> 16;
    if (root_off >= SQUASHFS_METADATA_SIZE) {
        return SQUASHFS_ERR_CORRUPT;
    }
    inode_span = sb.bytes_used - sb.inode_table_start;
    if (root_blk >= inode_span) {
        return SQUASHFS_ERR_CORRUPT;
    }

    *out = sb;
    return SQUASHFS_OK;
}

static int magic_at(const metta_u8 *img, metta_u64 size, metta_u64 off)
{
    if (!fits(size, off, 4u)) {
        return 0;
    }
    return img[off] == (metta_u8)'h' &&
           img[off + 1u] == (metta_u8)'s' &&
           img[off + 2u] == (metta_u8)'q' &&
           img[off + 3u] == (metta_u8)'s';
}

/*
 * Final del ELF64 little-endian, o 0 si no es un ELF que podamos medir.
 * No lee más allá de `size`: si la tabla de secciones no está mapeada,
 * usa e_shoff + e_shnum * e_shentsize, que cabe en los primeros 64 bytes.
 */
static int elf64_payload_end(const metta_u8 *img, metta_u64 size, metta_u64 *end_out)
{
    metta_u64 end = 0u;
    metta_u64 phoff;
    metta_u64 shoff;
    metta_u16 etype;
    metta_u16 ehsize;
    metta_u16 phentsize;
    metta_u16 phnum;
    metta_u16 shentsize;
    metta_u16 shnum;
    int any = 0;

    if (size < 64u) {
        return 0;
    }
    if (img[0] != 0x7fu || img[1] != (metta_u8)'E' ||
        img[2] != (metta_u8)'L' || img[3] != (metta_u8)'F') {
        return 0;
    }
    if (img[4] != 2u || img[5] != 1u) {
        return 0;
    }
    etype = rd16(img + 16);
    if (etype != 2u && etype != 3u) {
        return 0;
    }
    ehsize = rd16(img + 52);
    if (ehsize < 64u) {
        return 0;
    }
    phoff = rd64(img + 32);
    shoff = rd64(img + 40);
    phentsize = rd16(img + 54);
    phnum = rd16(img + 56);
    shentsize = rd16(img + 58);
    shnum = rd16(img + 60);

    if (phnum > 0u && phentsize >= 56u &&
        (metta_u64)phnum <= (UINT64_MAX / (metta_u64)phentsize) &&
        fits(size, phoff, (metta_u64)phentsize * (metta_u64)phnum)) {
        metta_u16 i;

        for (i = 0; i < phnum; i++) {
            const metta_u8 *ph = img + phoff + (metta_u64)i * phentsize;
            metta_u64 poff = rd64(ph + 8);
            metta_u64 filesz = rd64(ph + 32);

            if (filesz > UINT64_MAX - poff) {
                continue;
            }
            if (poff + filesz > end) {
                end = poff + filesz;
            }
            any = 1;
        }
    }

    if (shnum > 0u && shentsize >= 64u && shoff > 0u &&
        (metta_u64)shnum <= (UINT64_MAX - shoff) / (metta_u64)shentsize) {
        metta_u64 table = shoff + (metta_u64)shentsize * (metta_u64)shnum;
        metta_u64 shbytes = (metta_u64)shentsize * (metta_u64)shnum;

        if (table > end) {
            end = table;
        }
        any = 1;
        if (fits(size, shoff, shbytes)) {
            metta_u16 i;

            for (i = 0; i < shnum; i++) {
                const metta_u8 *sh = img + shoff + (metta_u64)i * shentsize;
                metta_u32 typ = rd32(sh + 4);
                metta_u64 soff = rd64(sh + 24);
                metta_u64 ssz = rd64(sh + 32);

                if (typ == 8u) {
                    continue; /* SHT_NOBITS: no ocupa bytes del archivo */
                }
                if (ssz > UINT64_MAX - soff) {
                    continue;
                }
                if (soff + ssz > end) {
                    end = soff + ssz;
                }
            }
        }
    }

    if (!any || end == 0u) {
        return 0;
    }
    *end_out = end;
    return 1;
}

static int find_at(const metta_u8 *img, metta_u64 size, metta_u64 begin, metta_u64 end,
                   metta_u64 *off_out)
{
    metta_u64 off;

    if (begin >= end || end > size) {
        return SQUASHFS_ERR_NOENT;
    }
    for (off = begin; off + 96u <= end; off++) {
        squashfs_sb_t sb;

        if (!magic_at(img, size, off)) {
            continue;
        }
        if (sb_parse(img + off, size - off, &sb) == SQUASHFS_OK) {
            *off_out = off;
            return SQUASHFS_OK;
        }
    }
    return SQUASHFS_ERR_NOENT;
}

static int find_superblock(const metta_u8 *img, metta_u64 size, metta_u64 *off_out)
{
    metta_u64 elf_end = 0u;
    metta_u64 window;
    metta_u64 limit;
    int rc;

    if (elf64_payload_end(img, size, &elf_end)) {
        if (elf_end > UINT64_MAX - SQUASHFS_ELF_PAD) {
            window = size;
        } else {
            window = elf_end + SQUASHFS_ELF_PAD;
        }
        if (window > size) {
            window = size;
        }
        rc = find_at(img, size, elf_end, window, off_out);
        if (rc == SQUASHFS_OK) {
            return SQUASHFS_OK;
        }
    }

    limit = size;
    if (limit > SQUASHFS_SCAN_LIMIT) {
        limit = SQUASHFS_SCAN_LIMIT;
    }
    return find_at(img, size, 0u, limit, off_out);
}

int squashfs_probe(const void *image, uint64_t size, uint64_t *offset_out)
{
    const metta_u8 *img;
    metta_u64 off = 0u;
    int rc;

    if (image == NULL || offset_out == NULL) {
        return SQUASHFS_ERR_INVAL;
    }
    if (size < 96u) {
        return SQUASHFS_ERR_NOENT;
    }
    img = (const metta_u8 *)image;
    rc = find_superblock(img, size, &off);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    *offset_out = off;
    return SQUASHFS_OK;
}

int squashfs_magic_valid(metta_u32 magic)
{
    return magic == SQUASHFS_MAGIC || magic == SQUASHFS_MAGIC_SWAP;
}

static metta_status_t squashfs_blob_read(const metta_io_blob_t *blob, metta_u64 offset,
                                         void *buf, metta_u64 len)
{
    if (blob == NULL || blob->read == NULL || buf == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (len == 0u) {
        return METTA_OK;
    }
    if (offset > blob->size || len > blob->size - offset) {
        return METTA_ERR_RANGE;
    }
    return blob->read(blob->ctx, offset, buf, len);
}

static metta_status_t status_from_squash(int rc)
{
    if (rc == SQUASHFS_OK) {
        return METTA_OK;
    }
    if (rc == SQUASHFS_ERR_NOENT) {
        return METTA_ERR_NOT_FOUND;
    }
    if (rc == SQUASHFS_ERR_INVAL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (rc == SQUASHFS_ERR_RANGE) {
        return METTA_ERR_RANGE;
    }
    return METTA_ERR_CORRUPT;
}

metta_status_t squashfs_probe_at(const metta_io_blob_t *blob, metta_u64 offset,
                                 squashfs_image_info_t *out)
{
    metta_u8 raw[96];
    squashfs_sb_t sb;
    metta_status_t st;
    int rc;

    if (out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (blob == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (offset >= blob->size || blob->size - offset < sizeof(raw)) {
        return METTA_ERR_RANGE;
    }

    st = squashfs_blob_read(blob, offset, raw, sizeof(raw));
    if (st != METTA_OK) {
        return st;
    }

    rc = sb_parse(raw, sizeof(raw), &sb);
    if (rc != SQUASHFS_OK) {
        return status_from_squash(rc);
    }
    if (sb.bytes_used > blob->size - offset) {
        return METTA_ERR_CORRUPT;
    }

    out->offset = offset;
    out->version_major = sb.version_major;
    out->version_minor = sb.version_minor;
    out->block_size = sb.block_size;
    out->compression = sb.compression;
    out->big_endian = 0;
    return METTA_OK;
}

static void meta_init(sq_meta_t *m, const metta_u8 *image, metta_u64 size,
                      metta_u64 block, metta_u32 offset)
{
    m->image = image;
    m->size = size;
    m->block = block;
    m->offset = offset;
    m->unc_len = 0u;
    m->next_block = 0u;
    m->loaded = 0;
    m->loaded_block = 0u;
}

static int meta_ensure(sq_meta_t *m)
{
    metta_u16 hdr;
    metta_u16 clen;
    int stored;
    int rc;
    uint64_t out_len = 0u;

    if (m->loaded && m->loaded_block == m->block) {
        return SQUASHFS_OK;
    }
    if (!fits(m->size, m->block, 2u)) {
        return SQUASHFS_ERR_RANGE;
    }
    hdr = rd16(m->image + m->block);
    clen = (metta_u16)(hdr & 0x7fffu);
    stored = (hdr & 0x8000u) != 0;
    if (clen == 0u || !fits(m->size, m->block + 2u, clen)) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (stored) {
        if (clen > SQUASHFS_METADATA_SIZE) {
            return SQUASHFS_ERR_CORRUPT;
        }
        memcpy(g_meta_unc, m->image + m->block + 2u, clen);
        m->unc_len = clen;
    } else {
        rc = squashfs_zlib_decompress(m->image + m->block + 2u, clen,
                                      g_meta_unc, SQUASHFS_METADATA_SIZE, &out_len);
        if (rc == SQUASHFS_ERR_NOSPC) {
            return SQUASHFS_ERR_CORRUPT;
        }
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (out_len == 0u || out_len > SQUASHFS_METADATA_SIZE) {
            return SQUASHFS_ERR_CORRUPT;
        }
        m->unc_len = (metta_u32)out_len;
    }
    m->loaded = 1;
    m->loaded_block = m->block;
    m->next_block = m->block + 2u + clen;
    return SQUASHFS_OK;
}

static int meta_read(sq_meta_t *m, void *dst, metta_u32 len)
{
    metta_u8 *out = (metta_u8 *)dst;
    metta_u32 guard = 0u;

    while (len > 0u) {
        metta_u32 n;
        int rc;

        if (++guard > SQUASHFS_METADATA_SIZE * 4u) {
            return SQUASHFS_ERR_CORRUPT;
        }
        rc = meta_ensure(m);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (m->offset > m->unc_len) {
            return SQUASHFS_ERR_CORRUPT;
        }
        if (m->offset == m->unc_len) {
            if (m->next_block <= m->block) {
                return SQUASHFS_ERR_CORRUPT;
            }
            m->block = m->next_block;
            m->offset = 0u;
            m->loaded = 0;
            continue;
        }
        n = m->unc_len - m->offset;
        if (n > len) {
            n = len;
        }
        memcpy(out, g_meta_unc + m->offset, n);
        m->offset += n;
        out += n;
        len -= n;
    }
    return SQUASHFS_OK;
}

static int emit_name(char *out, uint64_t cap, uint64_t *used,
                     const metta_u8 *name, metta_u32 nlen)
{
    uint64_t pos = *used;
    uint64_t extra = (pos == 0u) ? 0u : 1u;
    uint32_t i;

    if (nlen == 0u || nlen > SQUASHFS_NAME_MAX) {
        return SQUASHFS_ERR_CORRUPT;
    }
    for (i = 0; i < nlen; i++) {
        if (name[i] == (metta_u8)'\n' || name[i] == 0u) {
            return SQUASHFS_ERR_CORRUPT;
        }
    }
    if (pos > cap || extra > cap - pos || nlen > cap - pos - extra ||
        cap - pos - extra - nlen < 1u) {
        return SQUASHFS_ERR_NOSPC;
    }
    if (extra != 0u) {
        out[pos++] = '\n';
    }
    memcpy(out + pos, name, nlen);
    pos += nlen;
    out[pos] = '\0';
    *used = pos;
    return SQUASHFS_OK;
}

static int read_root_dir_loc(const metta_u8 *img, metta_u64 size, metta_u64 sq_off,
                             const squashfs_sb_t *sb, metta_u64 *dir_block,
                             metta_u32 *dir_off, metta_u64 *dir_size)
{
    sq_meta_t meta;
    metta_u8 base[16];
    metta_u8 rest[24];
    metta_u16 type;
    metta_u64 root_blk;
    metta_u32 root_off;
    metta_u64 inode_at;
    metta_u32 start_block;
    metta_u32 file_size;
    metta_u16 offset;
    int rc;

    root_blk = sb->root_inode >> 16;
    root_off = (metta_u32)(sb->root_inode & 0xffffu);
    if (sb->inode_table_start > UINT64_MAX - root_blk ||
        sq_off > UINT64_MAX - (sb->inode_table_start + root_blk)) {
        return SQUASHFS_ERR_CORRUPT;
    }
    inode_at = sq_off + sb->inode_table_start + root_blk;
    meta_init(&meta, img, size, inode_at, root_off);
    rc = meta_read(&meta, base, sizeof(base));
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    type = rd16(base);
    if (type == SQUASHFS_DIR_TYPE) {
        rc = meta_read(&meta, rest, 16u);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        start_block = rd32(rest);
        file_size = rd16(rest + 8);
        offset = rd16(rest + 10);
    } else if (type == SQUASHFS_LDIR_TYPE) {
        rc = meta_read(&meta, rest, 24u);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        file_size = rd32(rest + 4);
        start_block = rd32(rest + 8);
        offset = rd16(rest + 18);
    } else {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (offset >= SQUASHFS_METADATA_SIZE || file_size > SQUASHFS_ROOT_DIR_MAX) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (sb->directory_table_start > UINT64_MAX - start_block ||
        sq_off > UINT64_MAX - (sb->directory_table_start + start_block)) {
        return SQUASHFS_ERR_CORRUPT;
    }
    *dir_block = sq_off + sb->directory_table_start + start_block;
    *dir_off = offset;
    *dir_size = file_size;
    return SQUASHFS_OK;
}

static int list_directory(const metta_u8 *img, metta_u64 size, metta_u64 block,
                          metta_u32 offset, metta_u64 file_size,
                          char *out, uint64_t out_cap)
{
    sq_meta_t meta;
    metta_u64 remaining = file_size;
    uint64_t used = 0u;
    int rc;

    if (out_cap == 0u) {
        return SQUASHFS_ERR_INVAL;
    }
    /* Un directorio vacío legítimo anuncia file_size == 3, no 0. */
    if (file_size < 3u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    out[0] = '\0';
    meta_init(&meta, img, size, block, offset);

    /*
     * file_size cuenta los bytes de cabeceras y entradas. Un directorio
     * vacío mide 3. El formato arrastra hasta 3 bytes de holgura al final
     * (en el Firefox beta: 993 anunciados, 990 de entradas).
     */
    while (remaining >= SQUASHFS_DIR_HEADER) {
        metta_u8 hdr[SQUASHFS_DIR_HEADER];
        metta_u32 count;
        metta_u32 entries;
        metta_u32 e;

        rc = meta_read(&meta, hdr, SQUASHFS_DIR_HEADER);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        remaining -= SQUASHFS_DIR_HEADER;
        count = rd32(hdr);
        if (count >= SQUASHFS_DIR_COUNT_MAX) {
            return SQUASHFS_ERR_CORRUPT;
        }
        entries = count + 1u;
        for (e = 0; e < entries; e++) {
            metta_u8 ent[SQUASHFS_DIR_ENTRY];
            metta_u8 name[SQUASHFS_NAME_MAX];
            metta_u32 nlen;

            if (remaining < SQUASHFS_DIR_ENTRY) {
                return SQUASHFS_ERR_CORRUPT;
            }
            rc = meta_read(&meta, ent, SQUASHFS_DIR_ENTRY);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            nlen = (metta_u32)rd16(ent + 6) + 1u;
            if (nlen == 0u || nlen > SQUASHFS_NAME_MAX ||
                remaining < SQUASHFS_DIR_ENTRY + (metta_u64)nlen) {
                return SQUASHFS_ERR_CORRUPT;
            }
            rc = meta_read(&meta, name, nlen);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            remaining -= SQUASHFS_DIR_ENTRY + (metta_u64)nlen;
            rc = emit_name(out, out_cap, &used, name, nlen);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
        }
    }
    if (remaining > 3u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    return SQUASHFS_OK;
}

int squashfs_list_root(const void *image, uint64_t size, char *out, uint64_t out_cap)
{
    const metta_u8 *img;
    squashfs_sb_t sb;
    metta_u64 sq_off = 0u;
    metta_u64 dir_block = 0u;
    metta_u32 dir_off = 0u;
    metta_u64 dir_size = 0u;
    int rc;

    if (image == NULL || out == NULL || out_cap == 0u) {
        return SQUASHFS_ERR_INVAL;
    }
    out[0] = '\0';
    img = (const metta_u8 *)image;
    rc = squashfs_probe(img, size, &sq_off);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    rc = sb_parse(img + sq_off, size - sq_off, &sb);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    if (sb.compression != SQUASHFS_COMP_ZLIB) {
        return SQUASHFS_ERR_COMP;
    }
    rc = read_root_dir_loc(img, size, sq_off, &sb, &dir_block, &dir_off, &dir_size);
    if (rc != SQUASHFS_OK) {
        out[0] = '\0';
        return rc;
    }
    rc = list_directory(img, size, dir_block, dir_off, dir_size, out, out_cap);
    if (rc != SQUASHFS_OK) {
        out[0] = '\0';
    }
    return rc;
}

static int meta_skip(sq_meta_t *m, metta_u32 len)
{
    metta_u8 dump[64];

    while (len > 0u) {
        metta_u32 n = len;
        int rc;

        if (n > (metta_u32)sizeof(dump)) {
            n = (metta_u32)sizeof(dump);
        }
        rc = meta_read(m, dump, n);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        len -= n;
    }
    return SQUASHFS_OK;
}

static int name_eq(const metta_u8 *name, metta_u32 nlen, const char *want, metta_u32 wlen)
{
    metta_u32 i;

    if (nlen != wlen) {
        return 0;
    }
    for (i = 0; i < nlen; i++) {
        if (name[i] != (metta_u8)want[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 * Busca un nombre en un directorio ya localizado. La referencia del inodo
 * queda en coordenadas absolutas del archivo (bloque de metadatos + offset).
 */
static int dir_find(const metta_u8 *img, metta_u64 size, metta_u64 block,
                    metta_u32 offset, metta_u64 file_size, metta_u64 inode_table,
                    const char *want, metta_u32 wlen, metta_u64 *ino_block,
                    metta_u32 *ino_off)
{
    sq_meta_t meta;
    metta_u64 remaining = file_size;
    int rc;

    if (wlen == 0u || file_size < 3u) {
        return SQUASHFS_ERR_NOENT;
    }
    meta_init(&meta, img, size, block, offset);
    while (remaining >= SQUASHFS_DIR_HEADER) {
        metta_u8 hdr[SQUASHFS_DIR_HEADER];
        metta_u32 count;
        metta_u32 entries;
        metta_u32 start_block;
        metta_u32 e;

        rc = meta_read(&meta, hdr, SQUASHFS_DIR_HEADER);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        remaining -= SQUASHFS_DIR_HEADER;
        count = rd32(hdr);
        start_block = rd32(hdr + 4);
        if (count >= SQUASHFS_DIR_COUNT_MAX) {
            return SQUASHFS_ERR_CORRUPT;
        }
        entries = count + 1u;
        for (e = 0; e < entries; e++) {
            metta_u8 ent[SQUASHFS_DIR_ENTRY];
            metta_u8 name[SQUASHFS_NAME_MAX];
            metta_u32 nlen;
            metta_u32 ent_off;

            if (remaining < SQUASHFS_DIR_ENTRY) {
                return SQUASHFS_ERR_CORRUPT;
            }
            rc = meta_read(&meta, ent, SQUASHFS_DIR_ENTRY);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            nlen = (metta_u32)rd16(ent + 6) + 1u;
            ent_off = rd16(ent);
            if (nlen == 0u || nlen > SQUASHFS_NAME_MAX ||
                remaining < SQUASHFS_DIR_ENTRY + (metta_u64)nlen ||
                ent_off >= SQUASHFS_METADATA_SIZE) {
                return SQUASHFS_ERR_CORRUPT;
            }
            rc = meta_read(&meta, name, nlen);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            remaining -= SQUASHFS_DIR_ENTRY + (metta_u64)nlen;
            if (!name_eq(name, nlen, want, wlen)) {
                continue;
            }
            if (inode_table > UINT64_MAX - start_block) {
                return SQUASHFS_ERR_CORRUPT;
            }
            *ino_block = inode_table + start_block;
            *ino_off = ent_off;
            return SQUASHFS_OK;
        }
    }
    if (remaining > 3u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    return SQUASHFS_ERR_NOENT;
}

static int read_dir_at(const metta_u8 *img, metta_u64 size, metta_u64 ino_block,
                       metta_u32 ino_off, metta_u64 dir_base, metta_u64 *dir_block,
                       metta_u32 *dir_off, metta_u64 *dir_size)
{
    sq_meta_t meta;
    metta_u8 base[16];
    metta_u8 rest[24];
    metta_u16 type;
    metta_u32 start_block;
    metta_u32 file_size;
    metta_u16 offset;
    int rc;

    meta_init(&meta, img, size, ino_block, ino_off);
    rc = meta_read(&meta, base, sizeof(base));
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    type = rd16(base);
    if (type == SQUASHFS_DIR_TYPE) {
        rc = meta_read(&meta, rest, 16u);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        start_block = rd32(rest);
        file_size = rd16(rest + 8);
        offset = rd16(rest + 10);
    } else if (type == SQUASHFS_LDIR_TYPE) {
        rc = meta_read(&meta, rest, 24u);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        file_size = rd32(rest + 4);
        start_block = rd32(rest + 8);
        offset = rd16(rest + 18);
    } else {
        return SQUASHFS_ERR_NOENT;
    }
    if (offset >= SQUASHFS_METADATA_SIZE || file_size > SQUASHFS_ROOT_DIR_MAX) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (dir_base > UINT64_MAX - start_block) {
        return SQUASHFS_ERR_CORRUPT;
    }
    *dir_block = dir_base + start_block;
    *dir_off = offset;
    *dir_size = file_size;
    return SQUASHFS_OK;
}

static int next_component(const char **pp, char *name, metta_u32 *nlen)
{
    const char *p = *pp;
    metta_u32 n = 0u;

    while (*p == '/') {
        p++;
    }
    if (*p == '\0') {
        *pp = p;
        *nlen = 0u;
        return SQUASHFS_OK;
    }
    while (*p != '\0' && *p != '/') {
        if (n >= SQUASHFS_NAME_MAX) {
            return SQUASHFS_ERR_INVAL;
        }
        name[n++] = *p++;
    }
    name[n] = '\0';
    *pp = p;
    *nlen = n;
    return SQUASHFS_OK;
}

static int lookup_path(const metta_u8 *img, metta_u64 size, metta_u64 sq_off,
                       const squashfs_sb_t *sb, const char *path,
                       metta_u64 *ino_block, metta_u32 *ino_off)
{
    metta_u64 dir_block;
    metta_u32 dir_off;
    metta_u64 dir_size;
    metta_u64 inode_table;
    metta_u64 dir_base;
    const char *p = path;
    int rc;

    if (path == NULL || path[0] == '\0') {
        return SQUASHFS_ERR_INVAL;
    }
    if (sq_off > UINT64_MAX - sb->inode_table_start ||
        sq_off > UINT64_MAX - sb->directory_table_start) {
        return SQUASHFS_ERR_CORRUPT;
    }
    inode_table = sq_off + sb->inode_table_start;
    dir_base = sq_off + sb->directory_table_start;
    rc = read_root_dir_loc(img, size, sq_off, sb, &dir_block, &dir_off, &dir_size);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    for (;;) {
        char name[SQUASHFS_NAME_MAX + 1u];
        metta_u32 nlen = 0u;
        const char *saved;

        rc = next_component(&p, name, &nlen);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (nlen == 0u) {
            return SQUASHFS_ERR_INVAL;
        }
        if (nlen == 1u && name[0] == '.') {
            if (*p == '\0') {
                return SQUASHFS_ERR_INVAL;
            }
            continue;
        }
        if (nlen == 2u && name[0] == '.' && name[1] == '.') {
            return SQUASHFS_ERR_INVAL;
        }
        rc = dir_find(img, size, dir_block, dir_off, dir_size, inode_table,
                      name, nlen, ino_block, ino_off);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        saved = p;
        while (*saved == '/') {
            saved++;
        }
        if (*saved == '\0') {
            return SQUASHFS_OK;
        }
        rc = read_dir_at(img, size, *ino_block, *ino_off, dir_base,
                         &dir_block, &dir_off, &dir_size);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        p = saved;
    }
}

static int take_block(const metta_u8 *img, metta_u64 size, metta_u64 *pos,
                      metta_u32 raw, metta_u8 *dst, metta_u32 expect)
{
    metta_u32 csize = raw & ~SQUASHFS_COMP_BIT_BLOCK;
    int stored = (raw & SQUASHFS_COMP_BIT_BLOCK) != 0;
    uint64_t out_len = 0u;
    int rc;

    if (expect == 0u) {
        return SQUASHFS_OK;
    }
    if (csize == 0u) {
        memset(dst, 0, expect);
        return SQUASHFS_OK;
    }
    if (!fits(size, *pos, csize)) {
        return SQUASHFS_ERR_RANGE;
    }
    if (stored) {
        if (csize < expect) {
            return SQUASHFS_ERR_CORRUPT;
        }
        memcpy(dst, img + *pos, expect);
    } else {
        if (expect > SQUASHFS_DATA_MAX) {
            return SQUASHFS_ERR_NOSPC;
        }
        rc = squashfs_zlib_decompress(img + *pos, csize, g_data_unc,
                                      SQUASHFS_DATA_MAX, &out_len);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (out_len < expect) {
            return SQUASHFS_ERR_CORRUPT;
        }
        memcpy(dst, g_data_unc, expect);
    }
    *pos += csize;
    return SQUASHFS_OK;
}

static int frag_lookup(const metta_u8 *img, metta_u64 size, metta_u64 sq_off,
                       const squashfs_sb_t *sb, metta_u32 frag,
                       metta_u64 *abs_start, metta_u32 *raw_size)
{
    sq_meta_t meta;
    metta_u8 ent[16];
    metta_u32 index;
    metta_u32 slot;
    metta_u64 indexes;
    metta_u64 index_at;
    metta_u64 block;
    int rc;

    /*
     * El índice de fragmentos es una tabla cruda de u64 (no un bloque de
     * metadatos). Cada u64 apunta al bloque de metadatos con 512 entradas.
     */
    if (sb->fragment_table_start == SQUASHFS_INVALID_BLK || sb->fragments == 0u ||
        frag >= sb->fragments) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (sq_off > UINT64_MAX - sb->fragment_table_start) {
        return SQUASHFS_ERR_CORRUPT;
    }
    indexes = ((metta_u64)sb->fragments + (SQUASHFS_METADATA_SIZE / 16u) - 1u) /
              (SQUASHFS_METADATA_SIZE / 16u);
    index = frag / (SQUASHFS_METADATA_SIZE / 16u);
    slot = frag % (SQUASHFS_METADATA_SIZE / 16u);
    if ((metta_u64)index >= indexes) {
        return SQUASHFS_ERR_CORRUPT;
    }
    index_at = sq_off + sb->fragment_table_start + (metta_u64)index * 8u;
    if (!fits(size, index_at, 8u)) {
        return SQUASHFS_ERR_RANGE;
    }
    block = rd64(img + index_at);
    if (block == SQUASHFS_INVALID_BLK || block >= sb->bytes_used ||
        sq_off > UINT64_MAX - block) {
        return SQUASHFS_ERR_CORRUPT;
    }
    meta_init(&meta, img, size, sq_off + block, 0u);
    rc = meta_skip(&meta, slot * 16u);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    rc = meta_read(&meta, ent, 16u);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    block = rd64(ent);
    if (sq_off > UINT64_MAX - block) {
        return SQUASHFS_ERR_CORRUPT;
    }
    *abs_start = sq_off + block;
    *raw_size = rd32(ent + 8);
    return SQUASHFS_OK;
}

static int read_frag_slice(const metta_u8 *img, metta_u64 size, metta_u64 sq_off,
                           const squashfs_sb_t *sb, metta_u32 frag, metta_u32 frag_off,
                           metta_u8 *dst, metta_u32 tail)
{
    metta_u64 start = 0u;
    metta_u32 raw = 0u;
    metta_u32 csize;
    int stored;
    int rc;

    rc = frag_lookup(img, size, sq_off, sb, frag, &start, &raw);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    csize = raw & ~SQUASHFS_COMP_BIT_BLOCK;
    stored = (raw & SQUASHFS_COMP_BIT_BLOCK) != 0;
    if (csize == 0u || !fits(size, start, csize)) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (stored) {
        if ((metta_u64)frag_off + tail > csize) {
            return SQUASHFS_ERR_CORRUPT;
        }
        memcpy(dst, img + start + frag_off, tail);
        return SQUASHFS_OK;
    }
    {
        uint64_t out_len = 0u;

        rc = squashfs_zlib_decompress(img + start, csize, g_data_unc,
                                      SQUASHFS_DATA_MAX, &out_len);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if ((metta_u64)frag_off + tail > out_len) {
            return SQUASHFS_ERR_CORRUPT;
        }
        memcpy(dst, g_data_unc + frag_off, tail);
    }
    return SQUASHFS_OK;
}

static int read_reg_at(const metta_u8 *img, metta_u64 size, metta_u64 sq_off,
                       const squashfs_sb_t *sb, metta_u64 ino_block, metta_u32 ino_off,
                       void *dst, uint64_t dst_cap, uint64_t *out_len, int *is_link,
                       char *link, metta_u32 link_cap)
{
    sq_meta_t meta;
    metta_u8 base[16];
    metta_u16 type;
    metta_u64 file_size = 0u;
    metta_u64 data_pos = 0u;
    metta_u32 fragment = SQUASHFS_INVALID_FRAG;
    metta_u32 frag_off = 0u;
    metta_u64 blocks;
    metta_u64 full;
    metta_u32 tail;
    metta_u64 i;
    metta_u64 done;
    int has_frag;
    int rc;

    *is_link = 0;
    meta_init(&meta, img, size, ino_block, ino_off);
    rc = meta_read(&meta, base, sizeof(base));
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    type = rd16(base);
    if (type == SQUASHFS_SYMLINK_TYPE || type == SQUASHFS_LSYMLINK_TYPE) {
        metta_u8 hdr[8];
        metta_u32 slen;

        rc = meta_read(&meta, hdr, 8u);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        slen = rd32(hdr + 4);
        if (slen == 0u || slen >= link_cap) {
            return SQUASHFS_ERR_NOSPC;
        }
        rc = meta_read(&meta, link, slen);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        link[slen] = '\0';
        *is_link = 1;
        return SQUASHFS_OK;
    }
    if (type == SQUASHFS_REG_TYPE) {
        metta_u8 rest[16];
        metta_u32 start;

        rc = meta_read(&meta, rest, 16u);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        start = rd32(rest);
        fragment = rd32(rest + 4);
        frag_off = rd32(rest + 8);
        file_size = rd32(rest + 12);
        if (sq_off > UINT64_MAX - start) {
            return SQUASHFS_ERR_CORRUPT;
        }
        data_pos = sq_off + start;
    } else if (type == SQUASHFS_LREG_TYPE) {
        metta_u8 rest[40];
        metta_u64 start;

        rc = meta_read(&meta, rest, 40u);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        start = rd64(rest);
        file_size = rd64(rest + 8);
        fragment = rd32(rest + 28);
        frag_off = rd32(rest + 32);
        if (sq_off > UINT64_MAX - start) {
            return SQUASHFS_ERR_CORRUPT;
        }
        data_pos = sq_off + start;
    } else {
        return SQUASHFS_ERR_NOENT;
    }

    if (sb->block_size == 0u || sb->block_size > SQUASHFS_DATA_MAX) {
        return SQUASHFS_ERR_NOSPC;
    }
    if (out_len != NULL) {
        *out_len = file_size;
    }
    if (dst == NULL) {
        return SQUASHFS_OK;
    }
    if (dst_cap < file_size) {
        return SQUASHFS_ERR_NOSPC;
    }
    if (file_size == 0u) {
        return SQUASHFS_OK;
    }

    has_frag = fragment != SQUASHFS_INVALID_FRAG;
    full = file_size >> sb->block_log;
    tail = (metta_u32)(file_size & (sb->block_size - 1u));
    if (has_frag) {
        blocks = full;
    } else if (tail != 0u) {
        blocks = full + 1u;
    } else {
        blocks = full;
    }

    done = 0u;
    for (i = 0u; i < blocks; i++) {
        metta_u8 rawb[4];
        metta_u32 expect = sb->block_size;

        rc = meta_read(&meta, rawb, 4u);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (!has_frag && tail != 0u && i + 1u == blocks) {
            expect = tail;
        }
        rc = take_block(img, size, &data_pos, rd32(rawb),
                        (metta_u8 *)dst + done, expect);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        done += expect;
    }
    if (has_frag && tail != 0u) {
        rc = read_frag_slice(img, size, sq_off, sb, fragment, frag_off,
                             (metta_u8 *)dst + done, tail);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
    }
    return SQUASHFS_OK;
}

int squashfs_read_file(const void *image, uint64_t size, const char *path,
                       void *dst, uint64_t dst_cap, uint64_t *out_len)
{
    const metta_u8 *img;
    squashfs_sb_t sb;
    metta_u64 sq_off = 0u;
    metta_u64 ino_block = 0u;
    metta_u32 ino_off = 0u;
    char link[SQUASHFS_LINK_MAX];
    const char *cur;
    unsigned hops;
    int rc;

    if (image == NULL || path == NULL || out_len == NULL) {
        return SQUASHFS_ERR_INVAL;
    }
    if (dst == NULL && dst_cap != 0u) {
        return SQUASHFS_ERR_INVAL;
    }
    *out_len = 0u;
    img = (const metta_u8 *)image;
    rc = squashfs_probe(img, size, &sq_off);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    rc = sb_parse(img + sq_off, size - sq_off, &sb);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    if (sb.compression != SQUASHFS_COMP_ZLIB) {
        return SQUASHFS_ERR_COMP;
    }
    cur = path;
    for (hops = 0u; hops <= SQUASHFS_LINK_HOPS; hops++) {
        int is_link = 0;

        rc = lookup_path(img, size, sq_off, &sb, cur, &ino_block, &ino_off);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        rc = read_reg_at(img, size, sq_off, &sb, ino_block, ino_off,
                         dst, dst_cap, out_len, &is_link, link, sizeof(link));
        if (rc != SQUASHFS_OK || !is_link) {
            return rc;
        }
        cur = link;
    }
    return SQUASHFS_ERR_CORRUPT;
}

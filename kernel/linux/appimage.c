/*
 * MettaOS — reconocimiento AppImage tipo 1 / tipo 2 (SA-33).
 * Localiza el offset del SquashFS embebido y delega validación en squashfs_probe_at.
 */
#include <metta/types.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ELFMAG0  0x7fu
#define ELFMAG1  'E'
#define ELFMAG2  'L'
#define ELFMAG3  'F'

#define EM_X86_64  62u

#define APPIMAGE2_MAGIC_V1  0x41490100u /* AI\x01\x00 */
#define APPIMAGE2_MAGIC_V2  0x41490200u /* AI\x02\x00 */

#define APPIMAGE_SCAN_LIMIT   (512u * 1024u)
#define APPIMAGE_TYPE1_FOOTER 8u

/* Debe coincidir con kernel/fs/squashfs.c (header público pendiente). */
typedef metta_status_t (*metta_io_read_fn)(void *ctx, metta_u64 offset,
                                           void *buf, metta_u64 len);

typedef struct metta_io_blob {
    void *ctx;
    metta_io_read_fn read;
    metta_u64 size;
} metta_io_blob_t;

typedef struct squashfs_image_info {
    metta_u64 offset;
    metta_u16 version_major;
    metta_u16 version_minor;
    metta_u32 block_size;
    metta_u32 compression;
    int       big_endian;
} squashfs_image_info_t;

metta_status_t squashfs_probe_at(const metta_io_blob_t *blob, metta_u64 offset,
                                 squashfs_image_info_t *out);

typedef struct __attribute__((packed)) Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct __attribute__((packed)) appimage_header_v2 {
    metta_u32 magic;
    metta_u32 header_len;
    metta_u64 payload_offset;
    metta_u64 payload_length;
    metta_u64 assc_offset;
    metta_u64 assc_length;
    metta_u64 full_header_len;
} appimage_header_v2_t;

typedef enum {
    METTA_APPIMAGE_KIND_NONE = 0,
    METTA_APPIMAGE_TYPE1,
    METTA_APPIMAGE_TYPE2,
} metta_appimage_kind_t;

typedef struct metta_appimage_info {
    metta_appimage_kind_t kind;
    metta_u64             payload_offset;
    metta_u64             payload_length;
    metta_u64             header_offset;
    int                   elf64;
    metta_u16             elf_machine;
    squashfs_image_info_t squashfs;
} metta_appimage_info_t;

static metta_status_t appimage_blob_read(const metta_io_blob_t *blob, metta_u64 offset,
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

static int appimage_elf64_ok(const Elf64_Ehdr *eh)
{
    if (eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != ELFMAG1 ||
        eh->e_ident[2] != ELFMAG2 || eh->e_ident[3] != ELFMAG3) {
        return 0;
    }
    if (eh->e_ident[4] != 2 || eh->e_ident[5] != 1) {
        return 0; /* ELFCLASS64, ELFDATA2LSB */
    }
    if (eh->e_ehsize < sizeof(Elf64_Ehdr)) {
        return 0;
    }
    return 1;
}

static int appimage_magic_is_type2(metta_u32 magic)
{
    return magic == APPIMAGE2_MAGIC_V1 || magic == APPIMAGE2_MAGIC_V2;
}

static metta_status_t appimage_try_type2(const metta_io_blob_t *blob,
                                         metta_appimage_info_t *out)
{
    metta_u64 scan_end;
    metta_u64 pos;
    metta_status_t st;

    if (blob->size < sizeof(appimage_header_v2_t)) {
        return METTA_ERR_NOT_FOUND;
    }

    scan_end = blob->size;
    if (scan_end > APPIMAGE_SCAN_LIMIT) {
        scan_end = APPIMAGE_SCAN_LIMIT;
    }

    for (pos = 0u; pos + sizeof(appimage_header_v2_t) <= scan_end; pos += 4u) {
        appimage_header_v2_t hdr;

        st = appimage_blob_read(blob, pos, &hdr, sizeof(hdr));
        if (st != METTA_OK) {
            return st;
        }
        if (!appimage_magic_is_type2(hdr.magic)) {
            continue;
        }
        if (hdr.payload_offset == 0u || hdr.payload_offset >= blob->size) {
            continue;
        }
        if (hdr.payload_length != 0u &&
            hdr.payload_offset + hdr.payload_length > blob->size) {
            continue;
        }

        st = squashfs_probe_at(blob, hdr.payload_offset, &out->squashfs);
        if (st != METTA_OK) {
            continue;
        }

        out->kind = METTA_APPIMAGE_TYPE2;
        out->header_offset = pos;
        out->payload_offset = hdr.payload_offset;
        out->payload_length = hdr.payload_length;
        return METTA_OK;
    }

    return METTA_ERR_NOT_FOUND;
}

static metta_status_t appimage_try_type1(const metta_io_blob_t *blob,
                                         metta_appimage_info_t *out)
{
    metta_u64 footer_off;
    metta_u64 payload_off;
    metta_status_t st;

    if (blob->size < sizeof(Elf64_Ehdr) + APPIMAGE_TYPE1_FOOTER + 64u) {
        return METTA_ERR_NOT_FOUND;
    }

    footer_off = blob->size - APPIMAGE_TYPE1_FOOTER;
    st = appimage_blob_read(blob, footer_off, &payload_off, sizeof(payload_off));
    if (st != METTA_OK) {
        return st;
    }

    if (payload_off < sizeof(Elf64_Ehdr) || payload_off >= footer_off) {
        return METTA_ERR_NOT_FOUND;
    }

    st = squashfs_probe_at(blob, payload_off, &out->squashfs);
    if (st != METTA_OK) {
        return st;
    }

    out->kind = METTA_APPIMAGE_TYPE1;
    out->header_offset = 0u;
    out->payload_offset = payload_off;
    out->payload_length = (footer_off > payload_off) ? (footer_off - payload_off) : 0u;
    return METTA_OK;
}

/*
 * Dado un blob de archivo completo, detecta AppImage y el SquashFS embebido.
 */
metta_status_t appimage_probe(const metta_io_blob_t *blob, metta_appimage_info_t *out)
{
    Elf64_Ehdr eh;
    metta_status_t st;

    if (out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    if (blob == NULL || blob->size < sizeof(Elf64_Ehdr)) {
        return METTA_ERR_INVALID_ARG;
    }

    st = appimage_blob_read(blob, 0u, &eh, sizeof(eh));
    if (st != METTA_OK) {
        return st;
    }

    out->elf64 = appimage_elf64_ok(&eh);
    out->elf_machine = eh.e_machine;
    if (!out->elf64) {
        return METTA_ERR_CORRUPT;
    }

    st = appimage_try_type2(blob, out);
    if (st == METTA_OK) {
        return METTA_OK;
    }

    st = appimage_try_type1(blob, out);
    if (st == METTA_OK) {
        return METTA_OK;
    }

    return METTA_ERR_NOT_FOUND;
}
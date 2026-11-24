/*
 * MettaOS — inflater zlib para metadatos SquashFS.
 * El éxito exige un miembro zlib completo (cabecera, deflate y Adler-32).
 * Un bloque que no infla devuelve error; nunca se copia src a dst y se
 * pretende que eso sea la salida descomprimida.
 *
 * El orden de bits y la construcción canónica siguen RFC 1951: el primer
 * bit leído del byte (el menos significativo) es el bit alto del código
 * Huffman. Las restas de zhuff_decode son con signo a propósito.
 */
#include "fs/squashfs_zlib.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define Z_MAXBITS     15
#define Z_MAXSYMS     288
#define Z_MAXLENS     (288 + 32)

typedef struct zbit {
    const uint8_t *in;
    uint64_t       in_len;
    uint64_t       in_pos;
    uint32_t       bitbuf;
    int            bitcnt;
} zbit_t;

typedef struct zhuff {
    int   count[Z_MAXBITS + 1];
    short symbol[Z_MAXSYMS];
} zhuff_t;

/* Tablas reutilizadas por cada bloque. Una sola descompresión a la vez. */
static zhuff_t g_lit;
static zhuff_t g_dist;
static zhuff_t g_clen;
static uint8_t g_lens[Z_MAXLENS];

static const uint16_t k_len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10,
    11, 13, 15, 17,
    19, 23, 27, 31,
    35, 43, 51, 59,
    67, 83, 99, 115,
    131, 163, 195, 227,
    258
};

static const uint8_t k_len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1,
    2, 2, 2, 2,
    3, 3, 3, 3,
    4, 4, 4, 4,
    5, 5, 5, 5,
    0
};

static const uint16_t k_dist_base[30] = {
    1, 2, 3, 4,
    5, 7,
    9, 13,
    17, 25,
    33, 49,
    65, 97,
    129, 193,
    257, 385,
    513, 769,
    1025, 1537,
    2049, 3073,
    4097, 6145,
    8193, 12289,
    16385, 24577
};

static const uint8_t k_dist_extra[30] = {
    0, 0, 0, 0,
    1, 1,
    2, 2,
    3, 3,
    4, 4,
    5, 5,
    6, 6,
    7, 7,
    8, 8,
    9, 9,
    10, 10,
    11, 11,
    12, 12,
    13, 13
};

/* Orden de las longitudes del alfabeto de longitudes (RFC 1951). */
static const uint8_t k_clen_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static uint32_t zlib_adler32(const uint8_t *data, uint64_t len)
{
    uint32_t s1 = 1u;
    uint32_t s2 = 0u;
    uint64_t i;

    for (i = 0; i < len; i++) {
        s1 += data[i];
        if (s1 >= 65521u) {
            s1 -= 65521u;
        }
        s2 += s1;
        if (s2 >= 65521u) {
            s2 -= 65521u;
        }
        if (s2 >= 65521u) {
            s2 -= 65521u;
        }
    }
    return (s2 << 16) | s1;
}

static int zbits(zbit_t *z, int need, uint32_t *out)
{
    uint32_t val;

    if (need < 0 || need > 16) {
        return SQUASHFS_ERR_CORRUPT;
    }
    val = z->bitbuf;
    while (z->bitcnt < need) {
        if (z->in_pos >= z->in_len) {
            return SQUASHFS_ERR_CORRUPT;
        }
        val |= (uint32_t)z->in[z->in_pos++] << z->bitcnt;
        z->bitcnt += 8;
    }
    if (need == 0) {
        *out = 0u;
    } else {
        *out = val & ((1u << need) - 1u);
    }
    z->bitbuf = val >> need;
    z->bitcnt -= need;
    return SQUASHFS_OK;
}

static uint64_t zbit_tell(const zbit_t *z)
{
    return z->in_pos * 8u - (uint64_t)z->bitcnt;
}

/*
 * Construye la tabla canónica. Devuelve 0 si está completa, >0 si le faltan
 * códigos (válido mientras no se usen) y <0 si está sobrescrita.
 */
static int zhuff_build(zhuff_t *h, const uint8_t *length, int n)
{
    int len;
    int symbol;
    int left;
    int offs[Z_MAXBITS + 1];

    if (n <= 0 || n > Z_MAXSYMS) {
        return -1;
    }
    for (len = 0; len <= Z_MAXBITS; len++) {
        h->count[len] = 0;
    }
    for (symbol = 0; symbol < n; symbol++) {
        int L = (int)length[symbol];

        if (L > Z_MAXBITS) {
            return -1;
        }
        h->count[L]++;
    }
    if (h->count[0] == n) {
        return 0;
    }

    left = 1;
    for (len = 1; len <= Z_MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) {
            return left;
        }
    }

    offs[1] = 0;
    for (len = 1; len < Z_MAXBITS; len++) {
        offs[len + 1] = offs[len] + h->count[len];
    }
    for (symbol = 0; symbol < n; symbol++) {
        int L = (int)length[symbol];

        if (L != 0) {
            int pos = offs[L]++;

            if (pos < 0 || pos >= Z_MAXSYMS) {
                return -1;
            }
            h->symbol[pos] = (short)symbol;
        }
    }
    return left;
}

static int zhuff_decode(zbit_t *z, const zhuff_t *h, int *symbol_out)
{
    int len;
    int code = 0;
    int first = 0;
    int index = 0;

    for (len = 1; len <= Z_MAXBITS; len++) {
        uint32_t bit;
        int count;
        int rc;

        rc = zbits(z, 1, &bit);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        code |= (int)bit;
        count = h->count[len];
        if (code - count < first) {
            int pos = index + (code - first);

            if (pos < 0 || pos >= Z_MAXSYMS) {
                return SQUASHFS_ERR_CORRUPT;
            }
            *symbol_out = (int)h->symbol[pos];
            return SQUASHFS_OK;
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return SQUASHFS_ERR_CORRUPT;
}

static int zhuff_fixed(zhuff_t *lit, zhuff_t *dist)
{
    uint8_t lens[288];
    uint8_t dlens[30];
    int i;
    int rc;

    for (i = 0; i < 144; i++) {
        lens[i] = 8;
    }
    for (; i < 256; i++) {
        lens[i] = 9;
    }
    for (; i < 280; i++) {
        lens[i] = 7;
    }
    for (; i < 288; i++) {
        lens[i] = 8;
    }
    for (i = 0; i < 30; i++) {
        dlens[i] = 5;
    }
    rc = zhuff_build(lit, lens, 288);
    if (rc < 0) {
        return SQUASHFS_ERR_CORRUPT;
    }
    rc = zhuff_build(dist, dlens, 30);
    if (rc < 0) {
        return SQUASHFS_ERR_CORRUPT;
    }
    return SQUASHFS_OK;
}

static int zhuff_dynamic(zbit_t *z, zhuff_t *lit, zhuff_t *dist)
{
    uint32_t hlit;
    uint32_t hdist;
    uint32_t hclen;
    int nlen;
    int ndist;
    int ncode;
    int ntot;
    int index;
    int rc;
    uint8_t clens[19];

    rc = zbits(z, 5, &hlit);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    rc = zbits(z, 5, &hdist);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    rc = zbits(z, 4, &hclen);
    if (rc != SQUASHFS_OK) {
        return rc;
    }
    nlen = (int)hlit + 257;
    ndist = (int)hdist + 1;
    ncode = (int)hclen + 4;
    if (nlen < 257 || nlen > 288 || ndist < 1 || ndist > 32 || ncode < 4 || ncode > 19) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (nlen + ndist > Z_MAXLENS) {
        return SQUASHFS_ERR_CORRUPT;
    }

    memset(clens, 0, sizeof(clens));
    for (index = 0; index < ncode; index++) {
        uint32_t L;

        rc = zbits(z, 3, &L);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        clens[k_clen_order[index]] = (uint8_t)L;
    }
    rc = zhuff_build(&g_clen, clens, 19);
    if (rc < 0) {
        return SQUASHFS_ERR_CORRUPT;
    }

    memset(g_lens, 0, (size_t)(nlen + ndist));
    ntot = nlen + ndist;
    index = 0;
    while (index < ntot) {
        int sym;
        int rep;
        int prev;
        uint32_t extra;

        rc = zhuff_decode(z, &g_clen, &sym);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (sym < 16) {
            g_lens[index++] = (uint8_t)sym;
            continue;
        }
        if (sym == 16) {
            if (index == 0) {
                return SQUASHFS_ERR_CORRUPT;
            }
            rc = zbits(z, 2, &extra);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            rep = (int)extra + 3;
            prev = (int)g_lens[index - 1];
        } else if (sym == 17) {
            rc = zbits(z, 3, &extra);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            rep = (int)extra + 3;
            prev = 0;
        } else if (sym == 18) {
            rc = zbits(z, 7, &extra);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            rep = (int)extra + 11;
            prev = 0;
        } else {
            return SQUASHFS_ERR_CORRUPT;
        }
        if (rep < 0 || index > ntot - rep) {
            return SQUASHFS_ERR_CORRUPT;
        }
        while (rep > 0) {
            g_lens[index++] = (uint8_t)prev;
            rep--;
        }
    }

    rc = zhuff_build(lit, g_lens, nlen);
    if (rc < 0) {
        return SQUASHFS_ERR_CORRUPT;
    }
    rc = zhuff_build(dist, g_lens + nlen, ndist);
    if (rc < 0) {
        return SQUASHFS_ERR_CORRUPT;
    }
    return SQUASHFS_OK;
}

static int inflate_codes(zbit_t *z, const zhuff_t *lit, const zhuff_t *dist,
                         uint8_t *dst, uint64_t dst_cap, uint32_t *out_pos,
                         uint32_t window)
{
    for (;;) {
        int sym;
        int rc;

        rc = zhuff_decode(z, lit, &sym);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (sym < 256) {
            if ((uint64_t)(*out_pos) >= dst_cap) {
                return SQUASHFS_ERR_NOSPC;
            }
            dst[(*out_pos)++] = (uint8_t)sym;
            continue;
        }
        if (sym == 256) {
            return SQUASHFS_OK;
        }
        if (sym > 285) {
            return SQUASHFS_ERR_CORRUPT;
        }
        {
            int li = sym - 257;
            uint32_t extra = 0u;
            uint32_t length;
            uint32_t distance;
            int dsym;
            uint32_t i;

            if (k_len_extra[li] != 0) {
                rc = zbits(z, (int)k_len_extra[li], &extra);
                if (rc != SQUASHFS_OK) {
                    return rc;
                }
            }
            length = (uint32_t)k_len_base[li] + extra;
            rc = zhuff_decode(z, dist, &dsym);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            if (dsym < 0 || dsym > 29) {
                return SQUASHFS_ERR_CORRUPT;
            }
            extra = 0u;
            if (k_dist_extra[dsym] != 0) {
                rc = zbits(z, (int)k_dist_extra[dsym], &extra);
                if (rc != SQUASHFS_OK) {
                    return rc;
                }
            }
            distance = (uint32_t)k_dist_base[dsym] + extra;
            if (distance == 0u || distance > window || distance > *out_pos) {
                return SQUASHFS_ERR_CORRUPT;
            }
            if ((uint64_t)(*out_pos) + (uint64_t)length > dst_cap) {
                return SQUASHFS_ERR_NOSPC;
            }
            for (i = 0; i < length; i++) {
                dst[*out_pos] = dst[*out_pos - distance];
                (*out_pos)++;
            }
        }
    }
}

static int inflate_stored(zbit_t *z, uint8_t *dst, uint64_t dst_cap, uint32_t *out_pos)
{
    uint16_t len;
    uint16_t nlen;

    /* Alinea a byte: los bits sobrantes del encabezado no forman parte de LEN. */
    z->bitbuf = 0u;
    z->bitcnt = 0;
    if (z->in_pos + 4u > z->in_len) {
        return SQUASHFS_ERR_CORRUPT;
    }
    len = (uint16_t)z->in[z->in_pos] | (uint16_t)((uint16_t)z->in[z->in_pos + 1u] << 8);
    nlen = (uint16_t)z->in[z->in_pos + 2u] | (uint16_t)((uint16_t)z->in[z->in_pos + 3u] << 8);
    z->in_pos += 4u;
    if ((uint16_t)(len ^ 0xffffu) != nlen) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if ((uint64_t)len > z->in_len - z->in_pos) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if ((uint64_t)(*out_pos) + (uint64_t)len > dst_cap) {
        return SQUASHFS_ERR_NOSPC;
    }
    if (len > 0u) {
        memcpy(dst + *out_pos, z->in + z->in_pos, len);
    }
    z->in_pos += len;
    *out_pos += len;
    return SQUASHFS_OK;
}

static int inflate_member(const uint8_t *src, uint64_t src_len, uint8_t *dst,
                          uint64_t dst_cap, uint32_t window, uint32_t *out_pos)
{
    zbit_t z;
    int blocks = 0;

    z.in = src;
    z.in_len = src_len;
    z.in_pos = 0u;
    z.bitbuf = 0u;
    z.bitcnt = 0;
    *out_pos = 0u;

    do {
        uint32_t bfinal;
        uint32_t btype;
        uint64_t told;
        int rc;

        if (++blocks > 100000) {
            return SQUASHFS_ERR_CORRUPT;
        }
        told = zbit_tell(&z);
        rc = zbits(&z, 1, &bfinal);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        rc = zbits(&z, 2, &btype);
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (btype == 0u) {
            rc = inflate_stored(&z, dst, dst_cap, out_pos);
        } else if (btype == 1u) {
            rc = zhuff_fixed(&g_lit, &g_dist);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            rc = inflate_codes(&z, &g_lit, &g_dist, dst, dst_cap, out_pos, window);
        } else if (btype == 2u) {
            rc = zhuff_dynamic(&z, &g_lit, &g_dist);
            if (rc != SQUASHFS_OK) {
                return rc;
            }
            rc = inflate_codes(&z, &g_lit, &g_dist, dst, dst_cap, out_pos, window);
        } else {
            return SQUASHFS_ERR_CORRUPT;
        }
        if (rc != SQUASHFS_OK) {
            return rc;
        }
        if (zbit_tell(&z) <= told) {
            return SQUASHFS_ERR_CORRUPT;
        }
        if (bfinal) {
            break;
        }
    } while (1);

    if (z.bitcnt < 0 || z.bitcnt > 7) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if (z.in_pos != z.in_len) {
        return SQUASHFS_ERR_CORRUPT;
    }
    return SQUASHFS_OK;
}

int squashfs_zlib_decompress(const void *src, uint64_t src_len,
                             void *dst, uint64_t dst_cap, uint64_t *out_len)
{
    const uint8_t *in;
    uint8_t *out;
    unsigned cmf;
    unsigned flg;
    unsigned cinfo;
    uint32_t window;
    uint32_t produced = 0u;
    uint32_t expect;
    uint32_t got;
    int rc;

    if (out_len == NULL || src == NULL || (dst == NULL && dst_cap != 0u)) {
        return SQUASHFS_ERR_INVAL;
    }
    *out_len = 0u;
    /* Cabecera zlib (2) + deflate mínimo + Adler-32 (4). */
    if (src_len < 8u) {
        return SQUASHFS_ERR_CORRUPT;
    }

    in = (const uint8_t *)src;
    out = (uint8_t *)dst;
    cmf = in[0];
    flg = in[1];
    if ((cmf & 0x0fu) != 8u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    cinfo = cmf >> 4;
    if (cinfo > 7u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if ((((cmf << 8) | flg) % 31u) != 0u) {
        return SQUASHFS_ERR_CORRUPT;
    }
    if ((flg & 0x20u) != 0u) {
        return SQUASHFS_ERR_CORRUPT; /* diccionario predefinido: SquashFS no lo usa */
    }
    window = 1u << (cinfo + 8u);

    rc = inflate_member(in + 2, src_len - 6u, out, dst_cap, window, &produced);
    if (rc != SQUASHFS_OK) {
        return rc;
    }

    got = ((uint32_t)in[src_len - 4u] << 24) |
          ((uint32_t)in[src_len - 3u] << 16) |
          ((uint32_t)in[src_len - 2u] << 8) |
          (uint32_t)in[src_len - 1u];
    expect = zlib_adler32(out, produced);
    if (got != expect) {
        return SQUASHFS_ERR_CORRUPT;
    }
    *out_len = produced;
    return SQUASHFS_OK;
}

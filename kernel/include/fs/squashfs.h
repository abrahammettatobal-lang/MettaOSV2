/*
 * MettaOS — lector SquashFS 4.0 para el payload de un AppImage tipo 2.
 *
 * Identificadores de compresor, como en linux/include/uapi/linux/squashfs_fs.h
 * (no son los del paréntesis dudoso: xz es 4, no 2). Comprobado en el
 * AppImage de Firefox beta: el campo compression vale 1 y los bloques de
 * metadatos empiezan por la cabecera zlib 0x78 0xda.
 *   1 zlib  (mksquashfs lo llama gzip; el bloque es RFC 1950, no un miembro gzip)
 *   2 lzma
 *   3 lzo
 *   4 xz
 *   5 lz4
 *   6 zstd
 * Solo el id 1 se infla. Cualquier otro id se reconoce y squashfs_list_root
 * falla con SQUASHFS_ERR_COMP: no hay un camino que devuelva el bloque crudo
 * como si estuviera descomprimido.
 *
 * squashfs_probe no exige que los ~125 MiB estén mapeados: le basta el
 * superbloque. squashfs_list_root y squashfs_read_file necesitan el cuerpo
 * (tablas al final y bloques de datos). No se llama desde el arranque.
 */
#ifndef METTA_FS_SQUASHFS_H
#define METTA_FS_SQUASHFS_H

#include <stdint.h>

#define SQUASHFS_OK           0
#define SQUASHFS_ERR_NOENT    (-2)
#define SQUASHFS_ERR_INVAL    (-22)
#define SQUASHFS_ERR_NOSPC    (-28)
#define SQUASHFS_ERR_RANGE    (-34)
#define SQUASHFS_ERR_CORRUPT  (-74)
#define SQUASHFS_ERR_COMP     (-95)

#define SQUASHFS_COMP_ZLIB    1u
#define SQUASHFS_COMP_LZMA    2u
#define SQUASHFS_COMP_LZO     3u
#define SQUASHFS_COMP_XZ      4u
#define SQUASHFS_COMP_LZ4     5u
#define SQUASHFS_COMP_ZSTD    6u

/* 0 y offset del superbloque dentro de image. Negativo si no hay SquashFS 4.0. */
int squashfs_probe(const void *image, uint64_t size, uint64_t *offset_out);

/*
 * Nombres del directorio raíz, separados por '\n', sin barra final y con NUL.
 * 0 si la lista cabe entera. Si el retorno no es 0, out queda vacío
 * (out[0] == '\0') cuando out_cap > 0.
 */
int squashfs_list_root(const void *image, uint64_t size, char *out, uint64_t out_cap);

/*
 * Contenido de un archivo regular, camino desde la raíz ("AppRun",
 * "usr/bin/firefox"). Sigue como mucho cuatro enlaces simbólicos.
 * dst == NULL solo informa el tamaño en *out_len. Si dst no es NULL y
 * dst_cap es menor que el archivo, no escribe nada y devuelve NOSPC,
 * con el tamaño real en *out_len.
 */
int squashfs_read_file(const void *image, uint64_t size, const char *path,
                       void *dst, uint64_t dst_cap, uint64_t *out_len);

#endif /* METTA_FS_SQUASHFS_H */

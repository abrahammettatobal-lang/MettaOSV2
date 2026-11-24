/*
 * Prueba de anfitrión: carga un archivo del AppImage con el mismo lector
 * que enlaza el núcleo. No forma parte del arranque.
 *
 *   gcc -std=c11 -O2 -Wall -Wextra -I kernel/include \
 *       -o build/squashfs_load tests/squashfs_load.c \
 *       kernel/fs/squashfs.c kernel/fs/squashfs_zlib.c
 */
#include <stdio.h>
#include <stdlib.h>

#include "fs/squashfs.h"

int main(int argc, char **argv)
{
    const char *image_path = argc > 1 ? argv[1] : NULL;
    const char *want = argc > 2 ? argv[2] : "AppRun";
    FILE *fp;
    long sz;
    unsigned char *image;
    unsigned char *file = NULL;
    uint64_t need = 0;
    uint64_t got = 0;
    int rc;
    uint64_t show;
    uint64_t i;

    if (image_path == NULL) {
        fprintf(stderr, "uso: squashfs_load <AppImage> [camino]\n");
        return 2;
    }
    fp = fopen(image_path, "rb");
    if (fp == NULL) {
        perror(image_path);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (sz = ftell(fp)) < 0) {
        perror("ftell");
        fclose(fp);
        return 1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return 1;
    }
    image = (unsigned char *)malloc((size_t)sz);
    if (image == NULL || fread(image, 1, (size_t)sz, fp) != (size_t)sz) {
        perror("leer");
        free(image);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    rc = squashfs_read_file(image, (uint64_t)sz, want, NULL, 0, &need);
    printf("consulta %s -> %d, %llu bytes\n", want, rc, (unsigned long long)need);
    if (rc != SQUASHFS_OK) {
        free(image);
        return 1;
    }
    file = (unsigned char *)malloc(need ? (size_t)need : 1u);
    if (file == NULL) {
        fprintf(stderr, "sin memoria para %llu bytes\n", (unsigned long long)need);
        free(image);
        return 1;
    }
    rc = squashfs_read_file(image, (uint64_t)sz, want, file, need, &got);
    printf("lectura %s -> %d, %llu bytes\n", want, rc, (unsigned long long)got);
    if (rc != SQUASHFS_OK || got != need) {
        free(file);
        free(image);
        return 1;
    }
    show = got < 240u ? got : 240u;
    for (i = 0; i < show; i++) {
        unsigned char c = file[i];
        if (c == '\t' || c == '\n' || (c >= 32 && c < 127)) {
            fputc(c, stdout);
        } else {
            fputc('.', stdout);
        }
    }
    if (got > show) {
        fputc('\n', stdout);
    }
    fputc('\n', stdout);
    free(file);
    free(image);
    return 0;
}

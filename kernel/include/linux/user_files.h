#ifndef METTA_LINUX_USER_FILES_H
#define METTA_LINUX_USER_FILES_H

/*
 * Ficheros de usuario embebidos en el kernel (hello dinámico, ld.so y libc).
 * El fd solo guarda índice y offset; los bytes siguen en el blob .rodata.
 */
#include <stdint.h>

void linux_user_blobs_reserve(void);
void linux_user_hostlib_add(const char *path, const uint8_t *start, const uint8_t *end);
int linux_user_files_init(void);
void linux_user_set_exec_path(const char *path);
int linux_user_file_find(const char *path);          /* índice >=0 o -1 */
void linux_user_file_bind(int fd, int index);
void linux_user_file_detach(int fd);
int linux_user_file_owned(int fd);
int64_t linux_user_file_read(int fd, void *buf, uint64_t count);
int64_t linux_user_file_pread(int fd, void *buf, uint64_t count, uint64_t offset);
int64_t linux_user_file_lseek(int fd, int64_t offset, int whence); /* SEEK_SET=0, CUR=1, END=2; offset o -errno */
int linux_user_file_fstat(int fd, void *statbuf);    /* 0 o -errno */
int linux_user_file_stat_path(const char *path, void *statbuf);
const uint8_t *linux_user_file_data(int fd, uint64_t *size_out); /* puntero al blob; no mueve el offset */
int64_t linux_user_readlink_path(const char *path, char *buf, uint64_t bufsiz);

/* Carga el AppImage del disco y deja listo el SquashFS. 0 si está. */
int linux_user_appimage_load(void);
/* Archivo del SquashFS ya descomprimido, o NULL. */
const uint8_t *linux_user_squash_file(const char *rel, uint64_t *size_out);

#endif /* METTA_LINUX_USER_FILES_H */

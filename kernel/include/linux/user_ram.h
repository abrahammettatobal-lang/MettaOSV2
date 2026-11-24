#ifndef METTA_LINUX_USER_RAM_H
#define METTA_LINUX_USER_RAM_H

/* Directorios y archivos creados por el proceso (perfil de Firefox). */
#include <stdint.h>

void     linux_user_ram_init(void);
int      linux_user_ram_mkdir(const char *path);
int      linux_user_ram_stat(const char *path, void *statbuf);
int      linux_user_ram_create(int fd, const char *path, int trunc);
int      linux_user_ram_owned(int fd);
void     linux_user_ram_close(int fd);
int64_t  linux_user_ram_read(int fd, void *buf, uint64_t count);
int64_t  linux_user_ram_write(int fd, const void *buf, uint64_t count);
int64_t  linux_user_ram_lseek(int fd, int64_t offset, int whence);
int      linux_user_ram_fstat(int fd, void *statbuf);

#endif /* METTA_LINUX_USER_RAM_H */

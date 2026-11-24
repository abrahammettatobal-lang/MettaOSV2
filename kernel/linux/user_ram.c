/*
 * Archivos y directorios que el proceso crea en caliente.
 * Cabecera de 144 bytes igual que el stat de Linux x86_64.
 */
#include <linux/user_ram.h>

#include "syscall_errno.h"

#include <string.h>

#define RAM_FILES 24
#define RAM_BYTES (256u * 1024u)
#define RAM_DIRS  48
#define MODE_OFF  24
#define SIZE_OFF  48
#define MODE_REG  0100644u
#define MODE_DIR  0040755u

struct ram_file {
    int      used;
    char     path[160];
    uint32_t len;
    uint8_t  data[RAM_BYTES];
};

static struct ram_file g_files[RAM_FILES];
static char             g_dirs[RAM_DIRS][160];
static int              g_ndirs;
static int              g_fd_idx[256];
static uint32_t         g_fd_pos[256];

static void stat_put(void *statbuf, uint32_t mode, uint64_t size)
{
    uint8_t *st = statbuf;

    memset(st, 0, 144);
    memcpy(st + MODE_OFF, &mode, sizeof(mode));
    memcpy(st + SIZE_OFF, &size, sizeof(size));
}

static int path_eq(const char *a, const char *b)
{
    size_t i;

    for (i = 0; a[i] != '\0' && a[i] == b[i]; i++) {
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int dir_find(const char *path)
{
    int i;

    for (i = 0; i < g_ndirs; i++) {
        if (path_eq(g_dirs[i], path)) {
            return i;
        }
    }
    return -1;
}

static int file_find(const char *path)
{
    int i;

    for (i = 0; i < RAM_FILES; i++) {
        if (g_files[i].used && path_eq(g_files[i].path, path)) {
            return i;
        }
    }
    return -1;
}

void linux_user_ram_init(void)
{
    static const char *const seed[] = {
        "/home",
        "/home/usuario",
        "/tmp",
        "/tmp/.X11-unix",
        "/home/usuario/.mozilla",
        "/home/usuario/.cache"
    };
    static int ready;
    unsigned i;

    if (ready) {
        return;
    }
    ready = 1;
    g_ndirs = 0;
    memset(g_fd_idx, 0, sizeof(g_fd_idx));
    for (i = 0; i < sizeof(seed) / sizeof(seed[0]); i++) {
        (void)linux_user_ram_mkdir(seed[i]);
    }
}

int linux_user_ram_mkdir(const char *path)
{
    size_t n;

    if (path == NULL || path[0] == '\0') {
        return -(int)LINUX_EINVAL;
    }
    if (dir_find(path) >= 0 || file_find(path) >= 0) {
        return -(int)LINUX_EEXIST;
    }
    if (g_ndirs >= RAM_DIRS) {
        return -(int) LINUX_ENOMEM;
    }
    n = 0;
    while (path[n] != '\0' && n + 1u < sizeof(g_dirs[0])) {
        g_dirs[g_ndirs][n] = path[n];
        n++;
    }
    g_dirs[g_ndirs][n] = '\0';
    g_ndirs++;
    return 0;
}

int linux_user_ram_stat(const char *path, void *statbuf)
{
    int idx;

    if (path == NULL || statbuf == NULL) {
        return -(int)LINUX_EFAULT;
    }
    idx = file_find(path);
    if (idx >= 0) {
        stat_put(statbuf, MODE_REG, g_files[idx].len);
        return 0;
    }
    if (dir_find(path) >= 0) {
        stat_put(statbuf, MODE_DIR, 0);
        return 0;
    }
    return -(int)LINUX_ENOENT;
}

int linux_user_ram_create(int fd, const char *path, int trunc)
{
    int idx;
    size_t n;

    if (fd < 0 || fd >= 256 || path == NULL) {
        return -(int)LINUX_EBADF;
    }
    idx = file_find(path);
    if (idx < 0) {
        for (idx = 0; idx < RAM_FILES; idx++) {
            if (!g_files[idx].used) {
                break;
            }
        }
        if (idx == RAM_FILES) {
            return -(int) LINUX_ENOMEM;
        }
        g_files[idx].used = 1;
        g_files[idx].len = 0;
        n = 0;
        while (path[n] != '\0' && n + 1u < sizeof(g_files[idx].path)) {
            g_files[idx].path[n] = path[n];
            n++;
        }
        g_files[idx].path[n] = '\0';
    } else if (trunc) {
        g_files[idx].len = 0;
    }
    g_fd_idx[fd] = idx + 1;
    g_fd_pos[fd] = 0;
    return 0;
}

int linux_user_ram_owned(int fd)
{
    return fd >= 0 && fd < 256 && g_fd_idx[fd] != 0;
}

void linux_user_ram_close(int fd)
{
    if (linux_user_ram_owned(fd)) {
        g_fd_idx[fd] = 0;
        g_fd_pos[fd] = 0;
    }
}

int linux_user_ram_fstat(int fd, void *statbuf)
{
    int idx;

    if (!linux_user_ram_owned(fd) || statbuf == NULL) {
        return -(int)LINUX_EBADF;
    }
    idx = g_fd_idx[fd] - 1;
    stat_put(statbuf, MODE_REG, g_files[idx].len);
    return 0;
}

int64_t linux_user_ram_read(int fd, void *buf, uint64_t count)
{
    int idx;
    uint32_t pos;
    uint32_t n;

    if (!linux_user_ram_owned(fd) || buf == NULL) {
        return -(int64_t) LINUX_EBADF;
    }
    idx = g_fd_idx[fd] - 1;
    pos = g_fd_pos[fd];
    if (pos >= g_files[idx].len || count == 0u) {
        return 0;
    }
    n = g_files[idx].len - pos;
    if ((uint64_t)n > count) {
        n = (uint32_t)count;
    }
    memcpy(buf, g_files[idx].data + pos, n);
    g_fd_pos[fd] = pos + n;
    return (int64_t)n;
}

int64_t linux_user_ram_write(int fd, const void *buf, uint64_t count)
{
    int idx;
    uint32_t pos;
    uint32_t n;

    if (!linux_user_ram_owned(fd) || (buf == NULL && count != 0u)) {
        return -(int64_t) LINUX_EBADF;
    }
    idx = g_fd_idx[fd] - 1;
    pos = g_fd_pos[fd];
    if (pos >= RAM_BYTES) {
        return 0;
    }
    n = RAM_BYTES - pos;
    if ((uint64_t)n > count) {
        n = (uint32_t)count;
    }
    if (n != 0u) {
        memcpy(g_files[idx].data + pos, buf, n);
    }
    if (pos + n > g_files[idx].len) {
        g_files[idx].len = pos + n;
    }
    g_fd_pos[fd] = pos + n;
    return (int64_t)n;
}

int64_t linux_user_ram_lseek(int fd, int64_t offset, int whence)
{
    int idx;
    int64_t base;
    int64_t next;

    if (!linux_user_ram_owned(fd)) {
        return -(int64_t) LINUX_EBADF;
    }
    idx = g_fd_idx[fd] - 1;
    if (whence == 0) {
        base = 0;
    } else if (whence == 1) {
        base = (int64_t)g_fd_pos[fd];
    } else if (whence == 2) {
        base = (int64_t)g_files[idx].len;
    } else {
        return -(int64_t) LINUX_EINVAL;
    }
    next = base + offset;
    if (next < 0 || next > (int64_t) RAM_BYTES) {
        return -(int64_t) LINUX_EINVAL;
    }
    g_fd_pos[fd] = (uint32_t)next;
    return next;
}

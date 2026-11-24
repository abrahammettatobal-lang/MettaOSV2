/* MettaOS — ficheros embebidos que ve el userspace Linux. */
#include "linux/user_files.h"

#include "linux/runtime.h"

#include <fs/squashfs.h>
#include <fs/vfs.h>
#include <linux/usermode.h>
#include <linux/user_ram.h>
#include <log/klog.h>
#include <metta/pmm.h>
#include <metta/types.h>

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);

/* Símbolos del blob (user_blob.S). El final es exclusivo. */
extern const uint8_t dynhello_start[];
extern const uint8_t dynhello_end[];
extern const uint8_t ldlinux_start[];
extern const uint8_t ldlinux_end[];
extern const uint8_t libc_start[];
extern const uint8_t libc_end[];
extern const uint8_t libdl_start[];
extern const uint8_t libdl_end[];
extern const uint8_t libpthread_start[];
extern const uint8_t libpthread_end[];
extern const uint8_t libz_start[];
extern const uint8_t libz_end[];
extern const uint8_t libm_start[];
extern const uint8_t libm_end[];
extern const uint8_t libgcc_start[];
extern const uint8_t libgcc_end[];
extern const uint8_t libstdcxx_start[];
extern const uint8_t libstdcxx_end[];
extern const uint8_t librt_start[];
extern const uint8_t librt_end[];
extern const uint8_t appruntime_start[];
extern const uint8_t appruntime_end[];

extern char userblob_start[];
extern char userblob_end[];

#define USER_FILE_CAP   256
#define USER_FD_MAX     256
#define EXEC_PATH_MAX   96

#define LINUX_SEEK_SET  0
#define LINUX_SEEK_CUR  1
#define LINUX_SEEK_END  2

/* S_IFREG|0755 y S_IFCHR|0620, en octal como el struct stat de Linux. */
#define USER_MODE_REG   0100755u
#define USER_MODE_CHR   0020620u

/*
 * struct stat de Linux x86_64 (asm/stat.h). Sin packed: el alineado
 * natural ya deja st_mode en 24 y st_size en 48, tamaño 144.
 */
struct linux_user_kstat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    unsigned int  st_mode;
    unsigned int  st_uid;
    unsigned int  st_gid;
    unsigned int  __pad0;
    unsigned long st_rdev;
    long          st_size;
    long          st_blksize;
    long          st_blocks;
    unsigned long st_atime;
    unsigned long st_atime_nsec;
    unsigned long st_mtime;
    unsigned long st_mtime_nsec;
    unsigned long st_ctime;
    unsigned long st_ctime_nsec;
    long          __unused[3];
};

_Static_assert(sizeof(struct linux_user_kstat) == 144, "stat linux x86_64");
_Static_assert(offsetof(struct linux_user_kstat, st_mode) == 24, "st_mode");
_Static_assert(offsetof(struct linux_user_kstat, st_size) == 48, "st_size");

typedef struct {
    const char    *path;
    const uint8_t *data;
    uint64_t       size;
    int            disk;
} user_file_t;

static user_file_t g_files[USER_FILE_CAP];
static int         g_nfiles;
static int         g_inited;
static int         g_disk_fd = -1;

static int      g_fd_index[USER_FD_MAX];
static uint64_t g_fd_off[USER_FD_MAX];

/* Copia, no el puntero: un temporal no debe quedar colgando. */
static char g_exec_path[EXEC_PATH_MAX];

static int64_t err64(linux_errno_t err)
{
    return -(int64_t)err;
}

static uint64_t blob_len(const uint8_t *inicio, const uint8_t *fin)
{
    uintptr_t a = (uintptr_t)inicio;
    uintptr_t b = (uintptr_t)fin;

    if (b < a) {
        return 0;
    }
    return (uint64_t)(b - a);
}

static size_t blob_len_str(const char *s)
{
    size_t n = 0;

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static const char g_proc_fs[] =
    "nodev\tproc\nnodev\ttmpfs\nnodev\tdevtmpfs\n";
static const char g_proc_mounts[] =
    "proc /proc proc rw,nosuid,nodev,noexec 0 0\n"
    "tmpfs /tmp tmpfs rw 0 0\n";
static const char g_proc_cpu[] =
    "processor\t: 0\n"
    "vendor_id\t: GenuineIntel\n"
    "cpu family\t: 6\n"
    "model name\t: MettaOS\n"
    "flags\t\t: fpu sse sse2\n";
static const char g_proc_stat[] =
    "1 (firefox) R 0 0 0 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
static char g_maps[1536];

static void maps_add(size_t *n, const char *s)
{
    while (*s != '\0' && *n + 1u < sizeof(g_maps)) {
        g_maps[(*n)++] = *s++;
    }
}

static void maps_hex(size_t *n, uint64_t v)
{
    char tmp[17];
    int i = 16;

    tmp[16] = '\0';
    if (v == 0u) {
        maps_add(n, "0");
        return;
    }
    while (v != 0u && i > 0) {
        int d = (int)(v & 0xfu);
        tmp[--i] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
        v >>= 4;
    }
    maps_add(n, tmp + i);
}

static void maps_line(size_t *n, uint64_t lo, uint64_t hi, const char *perm, const char *name)
{
    if (hi <= lo) {
        return;
    }
    maps_hex(n, lo);
    maps_add(n, "-");
    maps_hex(n, hi);
    maps_add(n, " ");
    maps_add(n, perm);
    maps_add(n, " 00000000 00:00 0 ");
    maps_add(n, name);
    maps_add(n, "\n");
}

static void fill_maps(void)
{
    size_t n = 0;
    uint64_t end = linux_user_mmap_end();

    maps_line(&n, 0x02000000ull, 0x02b00000ull, "r-xp", "/home/usuario/firefox-bin");
    maps_line(&n, 0x10000000ull, 0x10040000ull, "r-xp", "/lib64/ld-linux-x86-64.so.2");
    maps_line(&n, 0x1fdf0000ull, 0x1fe00000ull, "rw-p", "[stack]");
    if (end > 0x40000000ull) {
        maps_line(&n, 0x40000000ull, end, "r-xp", "/home/usuario/firefox-bin");
    }
    g_maps[n] = '\0';
    for (n = 0; n < (size_t)g_nfiles; n++) {
        if (strcmp(g_files[n].path, "/proc/self/maps") == 0) {
            g_files[n].data = (const uint8_t *)g_maps;
            g_files[n].size = blob_len_str(g_maps);
            break;
        }
    }
}

static void registrar_bytes(const char *ruta, const uint8_t *data, uint64_t size)
{
    user_file_t *f;

    if (g_nfiles >= USER_FILE_CAP || ruta == NULL) {
        return;
    }
    f = &g_files[g_nfiles];
    f->path = ruta;
    f->data = data;
    f->size = size;
    f->disk = 0;
    g_nfiles++;
}

static void registrar(const char *ruta, const uint8_t *inicio, const uint8_t *fin)
{
    registrar_bytes(ruta, inicio, blob_len(inicio, fin));
}

void linux_user_hostlib_add(const char *path, const uint8_t *start, const uint8_t *end)
{
    registrar(path, start, end);
}

void linux_user_register_hostlibs(void);

/* Imagen del AppImage y caché descomprimida, por encima del BSS y del userblob. */
#define APP_RAM      0x70000000ull
#define APP_RAM_MAX  (136ull * 1024ull * 1024ull)
#define SQ_RAM       0x80000000ull
#define SQ_RAM_MAX   (420ull * 1024ull * 1024ull)

static uint8_t *g_app_image;
static uint64_t g_app_size;
static uint8_t *g_sq_bump;
static uint8_t *g_sq_end;
static char     g_sq_names[USER_FILE_CAP][128];
static int      g_sq_used;

static int prefix_at(const char *path, const char *prefix, const char **rel_out)
{
    size_t i;

    for (i = 0; prefix[i] != '\0'; i++) {
        if (path[i] != prefix[i]) {
            return 0;
        }
    }
    if (path[i] == '\0') {
        return 0;
    }
    *rel_out = path + i;
    return 1;
}

static const char *squash_relative(const char *path)
{
    const char *rel = NULL;

    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    if (prefix_at(path, "/home/usuario/", &rel)) {
        if (strcmp(rel, "firefox") == 0 || strcmp(rel, "dynhello") == 0) {
            return NULL;
        }
        if (prefix_at(path, "/home/usuario/Documentos/", &rel)) {
            return NULL;
        }
        return rel;
    }
    if (path[0] != '/') {
        return path;
    }
    return NULL;
}

int linux_user_appimage_load(void)
{
    uint64_t off;
    uint64_t size;
    uint8_t *dst;

    if (g_app_image != NULL) {
        return 0;
    }
    if (linux_user_files_init() != 0) {
        return -1;
    }
    if (g_disk_fd < 0) {
        return -1;
    }
    size = vfs_fd_size(g_disk_fd);
    if (size < 64u || size > APP_RAM_MAX) {
        return -1;
    }
    if (pmm_mark_reserved((metta_phys_addr_t)APP_RAM, APP_RAM_MAX) != METTA_OK) {
        return -1;
    }
    if (pmm_mark_reserved((metta_phys_addr_t)SQ_RAM, SQ_RAM_MAX) != METTA_OK) {
        return -1;
    }
    dst = (uint8_t *)(uintptr_t)APP_RAM;
    if (vfs_seek(g_disk_fd, 0) != METTA_OK) {
        return -1;
    }
    off = 0;
    while (off < size) {
        uint64_t chunk = size - off;
        metta_ssize_t got;

        if (chunk > 65536u) {
            chunk = 65536u;
        }
        got = vfs_read(g_disk_fd, dst + off, chunk);
        if (got <= 0) {
            int retry;

            for (retry = 0; retry < 6 && got <= 0; retry++) {
                if (vfs_seek(g_disk_fd, off) != METTA_OK) {
                    break;
                }
                got = vfs_read(g_disk_fd, dst + off, chunk);
            }
        }
        if (got <= 0) {
            KLOG_INFO("firefox: disco falló en %llu (lectura %lld)\n",
                      (unsigned long long)off, (long long)got);
            return -1;
        }
        off += (uint64_t)got;
        if ((off & 0x7FFFFFu) == 0u) {
            KLOG_INFO("firefox: imagen %llu/%llu\n",
                      (unsigned long long)off,
                      (unsigned long long)size);
        }
    }
    g_app_image = dst;
    g_app_size  = size;
    g_sq_bump   = (uint8_t *)(uintptr_t)SQ_RAM;
    g_sq_end    = g_sq_bump + SQ_RAM_MAX;
    KLOG_INFO("firefox: imagen lista, %llu bytes\n", (unsigned long long)size);
    return 0;
}

const uint8_t *linux_user_squash_file(const char *rel, uint64_t *size_out)
{
    uint64_t need = 0;
    uint8_t *dst;
    int rc;
    int i;

    if (rel == NULL || g_app_image == NULL) {
        return NULL;
    }
    for (i = 0; i < g_nfiles; i++) {
        const char *have = squash_relative(g_files[i].path);

        if (have != NULL && strcmp(have, rel) == 0 && g_files[i].data != NULL) {
            if (size_out != NULL) {
                *size_out = g_files[i].size;
            }
            return g_files[i].data;
        }
    }
    rc = squashfs_read_file(g_app_image, g_app_size, rel, NULL, 0, &need);
    if (rc != SQUASHFS_OK) {
        return NULL;
    }
    dst = (uint8_t *)(((uintptr_t)g_sq_bump + 15u) & ~(uintptr_t)15u);
    if (dst >= g_sq_end || need > (uint64_t)(g_sq_end - dst)) {
        KLOG_INFO("firefox: sin caché para %s (%llu)\n",
                  rel, (unsigned long long)need);
        return NULL;
    }
    KLOG_INFO("firefox: leer %s (%llu bytes)\n", rel, (unsigned long long)need);
    rc = squashfs_read_file(g_app_image, g_app_size, rel, dst, need, &need);
    if (rc != SQUASHFS_OK) {
        KLOG_INFO("firefox: fallo al leer %s (%d)\n", rel, rc);
        return NULL;
    }
    g_sq_bump = dst + need;
    if (size_out != NULL) {
        *size_out = need;
    }
    return dst;
}

/* El AppImage completo vive en el disco MFS. Si está, sustituye el prefijo ELF de la RAM. */
static void preferir_disco(void)
{
    int fd = -1;
    metta_u64 sz;
    int i;
    int firefox = -1;

    if (vfs_open("/home/usuario/firefox", VFS_O_RDONLY, &fd) != METTA_OK) {
        return;
    }
    sz = vfs_fd_size(fd);
    if (sz == 0u) {
        (void)vfs_close(fd);
        return;
    }
    g_disk_fd = fd;
    for (i = 0; i < g_nfiles; i++) {
        if (strcmp(g_files[i].path, "/home/usuario/firefox") == 0) {
            firefox = i;
            break;
        }
    }
    if (firefox >= 0) {
        g_files[firefox].data = NULL;
        g_files[firefox].size = sz;
        g_files[firefox].disk = 1;
    }
    if (g_nfiles < USER_FILE_CAP) {
        user_file_t *f = &g_files[g_nfiles++];

        f->path = "/home/usuario/Documentos/Firefox.AppImage";
        f->data = NULL;
        f->size = sz;
        f->disk = 1;
    }
}

/* Componente tras la última barra. Vacío si la ruta acaba en /. */
static const char *basename_de(const char *ruta)
{
    const char *ultimo = NULL;
    const char *p;

    if (ruta == NULL) {
        return "";
    }
    for (p = ruta; *p != '\0'; p++) {
        if (*p == '/') {
            ultimo = p;
        }
    }
    if (ultimo == NULL) {
        return ruta;
    }
    return ultimo + 1;
}

/* Sin fallback para basename vacío o de un solo carácter. */
static int basename_corto(const char *base)
{
    if (base == NULL || base[0] == '\0' || base[1] == '\0') {
        return 1;
    }
    return 0;
}

static int fd_index(int fd)
{
    if (!g_inited || fd < 0 || fd >= USER_FD_MAX) {
        return -1;
    }
    return g_fd_index[fd];
}

static void stat_fichero(struct linux_user_kstat *st, int index)
{
    uint64_t sz = g_files[index].size;

    memset(st, 0, sizeof(*st));
    st->st_dev = 1;
    st->st_ino = (unsigned long)index + 1ul;
    st->st_nlink = 1;
    st->st_mode = USER_MODE_REG;
    st->st_size = (long)sz;
    st->st_blksize = 4096;
    st->st_blocks = (long)((sz + 511u) / 512u);
}

void linux_user_blobs_reserve(void)
{
    uintptr_t base = (uintptr_t)userblob_start & ~(uintptr_t)0xFFFu;
    uintptr_t end = ((uintptr_t)userblob_end + 0xFFFu) & ~(uintptr_t)0xFFFu;

    if (end <= base) {
        return;
    }
    (void)pmm_mark_reserved((metta_phys_addr_t)base, (metta_u64)(end - base));
}

int linux_user_files_init(void)
{
    int i;

    if (g_inited) {
        return 0;
    }
    for (i = 0; i < USER_FD_MAX; i++) {
        g_fd_index[i] = -1;
        g_fd_off[i] = 0;
    }
    g_nfiles = 0;

    registrar("/home/usuario/dynhello", dynhello_start, dynhello_end);
    /* El mismo ld.so bajo las tres rutas que prueba el intérprete. */
    registrar("/lib64/ld-linux-x86-64.so.2", ldlinux_start, ldlinux_end);
    registrar("/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2",
              ldlinux_start, ldlinux_end);
    registrar("/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2",
              ldlinux_start, ldlinux_end);
    registrar("/lib/x86_64-linux-gnu/libc.so.6", libc_start, libc_end);
    registrar("/usr/lib/x86_64-linux-gnu/libc.so.6", libc_start, libc_end);
    registrar("/lib/x86_64-linux-gnu/libdl.so.2", libdl_start, libdl_end);
    registrar("/lib/x86_64-linux-gnu/libpthread.so.0", libpthread_start, libpthread_end);
    registrar("/lib/x86_64-linux-gnu/libz.so.1", libz_start, libz_end);
    registrar("/lib/x86_64-linux-gnu/libm.so.6", libm_start, libm_end);
    registrar("/usr/lib/x86_64-linux-gnu/libm.so.6", libm_start, libm_end);
    registrar("/lib/x86_64-linux-gnu/libgcc_s.so.1", libgcc_start, libgcc_end);
    registrar("/usr/lib/x86_64-linux-gnu/libgcc_s.so.1", libgcc_start, libgcc_end);
    registrar("/lib/x86_64-linux-gnu/libstdc++.so.6", libstdcxx_start, libstdcxx_end);
    registrar("/usr/lib/x86_64-linux-gnu/libstdc++.so.6", libstdcxx_start, libstdcxx_end);
    registrar("/lib/x86_64-linux-gnu/librt.so.1", librt_start, librt_end);
    registrar("/usr/lib/x86_64-linux-gnu/librt.so.1", librt_start, librt_end);
    linux_user_register_hostlibs();
    registrar("/home/usuario/firefox", appruntime_start, appruntime_end);
    registrar_bytes("/proc/filesystems", (const uint8_t *)g_proc_fs, blob_len_str(g_proc_fs));
    registrar_bytes("/proc/mounts", (const uint8_t *)g_proc_mounts, blob_len_str(g_proc_mounts));
    registrar_bytes("/proc/cpuinfo", (const uint8_t *)g_proc_cpu, blob_len_str(g_proc_cpu));
    registrar_bytes("/proc/self/stat", (const uint8_t *)g_proc_stat, blob_len_str(g_proc_stat));
    registrar_bytes("/proc/self/maps", (const uint8_t *)g_maps, 0);
    preferir_disco();

    g_inited = 1;
    return 0;
}

void linux_user_set_exec_path(const char *path)
{
    size_t i;

    if (path == NULL) {
        g_exec_path[0] = '\0';
        return;
    }
    for (i = 0; i + 1u < sizeof(g_exec_path) && path[i] != '\0'; i++) {
        g_exec_path[i] = path[i];
    }
    g_exec_path[i] = '\0';
}

int linux_user_file_find(const char *path)
{
    int i;
    const char *base;

    if (!g_inited || path == NULL) {
        return -1;
    }
    /* El runtime del AppImage abre su propio ejecutable por aquí. */
    if (strcmp(path, "/proc/self/exe") == 0 && g_exec_path[0] != '\0') {
        path = g_exec_path;
    }
    if (strcmp(path, "/proc/self/maps") == 0) {
        fill_maps();
    }
    for (i = 0; i < g_nfiles; i++) {
        if (strcmp(path, g_files[i].path) == 0) {
            return i;
        }
    }
    base = basename_de(path);
    if (basename_corto(base)) {
        return -1;
    }
    for (i = 0; i < g_nfiles; i++) {
        const char *reg = basename_de(g_files[i].path);

        if (basename_corto(reg)) {
            continue;
        }
        if (strcmp(base, reg) == 0) {
            return i;
        }
    }
    if (g_app_image != NULL && g_nfiles < USER_FILE_CAP && g_sq_used < USER_FILE_CAP) {
        const char *rel = squash_relative(path);
        const uint8_t *data;
        uint64_t sz = 0;
        char *slot;
        size_t n;

        if (rel != NULL) {
            int before = g_nfiles;

            data = linux_user_squash_file(rel, &sz);
            if (data != NULL && before < USER_FILE_CAP) {
                slot = g_sq_names[g_sq_used++];
                for (n = 0; n + 1u < sizeof(g_sq_names[0]) && path[n] != '\0'; n++) {
                    slot[n] = path[n];
                }
                slot[n] = '\0';
                registrar_bytes(slot, data, sz);
                if (g_nfiles > before) {
                    return g_nfiles - 1;
                }
            }
        }
    }
    return -1;
}

void linux_user_file_bind(int fd, int index)
{
    if (!g_inited || fd < 0 || fd >= USER_FD_MAX) {
        return;
    }
    if (index < 0 || index >= g_nfiles) {
        return;
    }
    g_fd_index[fd] = index;
    g_fd_off[fd] = 0;
}

void linux_user_file_detach(int fd)
{
    if (!g_inited || fd < 0 || fd >= USER_FD_MAX) {
        return;
    }
    g_fd_index[fd] = -1;
    g_fd_off[fd] = 0;
}

int linux_user_file_owned(int fd)
{
    return fd_index(fd) >= 0;
}

static int64_t copy_at(int index, uint64_t off, void *buf, uint64_t count)
{
    uint64_t size = g_files[index].size;
    uint64_t n;
    metta_ssize_t got;

    if (off >= size || count == 0u) {
        return 0;
    }
    n = size - off;
    if (n > count) {
        n = count;
    }
    if (buf == NULL) {
        return err64(LINUX_EFAULT);
    }
    if (g_files[index].disk) {
        if (g_disk_fd < 0 || vfs_seek(g_disk_fd, off) != METTA_OK) {
            return err64(LINUX_EIO);
        }
        got = vfs_read(g_disk_fd, buf, n);
        if (got < 0) {
            return err64(LINUX_EIO);
        }
        return got;
    }
    memcpy(buf, g_files[index].data + off, (size_t)n);
    return (int64_t)n;
}

int64_t linux_user_file_read(int fd, void *buf, uint64_t count)
{
    int index;
    int64_t n;
    uint64_t off;

    index = fd_index(fd);
    if (index < 0 || index >= g_nfiles) {
        return err64(LINUX_EBADF);
    }
    off = g_fd_off[fd];
    n = copy_at(index, off, buf, count);
    if (n > 0) {
        g_fd_off[fd] = off + (uint64_t)n;
    }
    return n;
}

int64_t linux_user_file_pread(int fd, void *buf, uint64_t count, uint64_t offset)
{
    int index;

    index = fd_index(fd);
    if (index < 0 || index >= g_nfiles) {
        return err64(LINUX_EBADF);
    }
    return copy_at(index, offset, buf, count);
}

int64_t linux_user_file_lseek(int fd, int64_t offset, int whence)
{
    int index;
    uint64_t size;
    int64_t base;
    int64_t next;

    index = fd_index(fd);
    if (index < 0 || index >= g_nfiles) {
        return err64(LINUX_EBADF);
    }
    size = g_files[index].size;
    if (whence == LINUX_SEEK_SET) {
        base = 0;
    } else if (whence == LINUX_SEEK_CUR) {
        base = (int64_t)g_fd_off[fd];
    } else if (whence == LINUX_SEEK_END) {
        if (size > (uint64_t)INT64_MAX) {
            return err64(LINUX_EINVAL);
        }
        base = (int64_t)size;
    } else {
        return err64(LINUX_EINVAL);
    }
    if (__builtin_add_overflow(base, offset, &next) || next < 0) {
        return err64(LINUX_EINVAL);
    }
    g_fd_off[fd] = (uint64_t)next;
    return next;
}

int linux_user_file_fstat(int fd, void *statbuf)
{
    struct linux_user_kstat *st;
    int index;

    if (statbuf == NULL) {
        return -(int)LINUX_EFAULT;
    }
    st = statbuf;
    index = fd_index(fd);
    if (index >= 0 && index < g_nfiles) {
        stat_fichero(st, index);
        return 0;
    }
    if (linux_user_ram_owned(fd)) {
        return linux_user_ram_fstat(fd, statbuf);
    }
    /* glibc/isatty: stdin, stdout y stderr sin fichero asociado son tty. */
    if (fd == 0 || fd == 1 || fd == 2) {
        memset(st, 0, sizeof(*st));
        st->st_mode = USER_MODE_CHR;
        return 0;
    }
    return -(int)LINUX_EBADF;
}

int linux_user_file_stat_path(const char *path, void *statbuf)
{
    int index;

    if (statbuf == NULL) {
        return -(int)LINUX_EFAULT;
    }
    index = linux_user_file_find(path);
    if (index < 0) {
        return linux_user_ram_stat(path, statbuf);
    }
    stat_fichero(statbuf, index);
    return 0;
}

const uint8_t *linux_user_file_data(int fd, uint64_t *size_out)
{
    int index;

    index = fd_index(fd);
    if (index < 0 || index >= g_nfiles) {
        return NULL;
    }
    if (size_out != NULL) {
        *size_out = g_files[index].size;
    }
    if (g_files[index].disk) {
        return NULL;
    }
    return g_files[index].data;
}

int64_t linux_user_readlink_path(const char *path, char *buf, uint64_t bufsiz)
{
    size_t len;
    uint64_t n;

    if (path == NULL || strcmp(path, "/proc/self/exe") != 0) {
        return err64(LINUX_ENOENT);
    }
    len = strlen(g_exec_path);
    n = (uint64_t)len;
    if (n > bufsiz) {
        n = bufsiz;
    }
    if (n > 0) {
        if (buf == NULL) {
            return err64(LINUX_EFAULT);
        }
        memcpy(buf, g_exec_path, (size_t)n);
    }
    return (int64_t)n;
}

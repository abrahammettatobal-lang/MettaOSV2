/*
 * Syscalls fuertes que ld-linux y la libc piden al arrancar un hello dinámico.
 * Sustituyen los stubs débiles de syscall_bridge.c. read/write/open/close/exit
 * siguen definidos allí (el hello estático no pasa por aquí).
 */
#include "syscall_linux.h"

#include <linux/usermode.h>
#include <linux/user_files.h>
#include <linux/user_ram.h>
#include <linux/xdisplay.h>

#include <interrupt/pit.h>
#include <log/klog.h>
#include <metta/pmm.h>
#include <metta/vmm.h>

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
size_t strlen(const char *s);

/* El cargador define el flag y declara el trazador en linux/usermode.h. */
extern int linux_user_trace;

#define USER_LO           0x00400000ull
/* libxul.so son ~180 MiB. El hueco bajo el BSS (512 MiB) no alcanza. */
#define USER_HI           0x70000000ull
#define MMAP_BUMP_BASE    0x40000000ull
#define MMAP_BUMP_LIMIT   0x70000000ull
#define BRK_LO            0x04000000ull
#define BRK_HI            0x08000000ull
#define PAGE_SIZE         4096ull

#define MAP_PRIVATE           0x02
#define MAP_FIXED             0x10
#define MAP_ANONYMOUS         0x20
#define MAP_FIXED_NOREPLACE   0x100000

#define ARCH_SET_FS       0x1002
#define ARCH_GET_FS       0x1003
#define MSR_FS_BASE       0xC0000100u

#define TCGETS            0x5401u
#define TIOCGWINSZ        0x5413u

#define SEEK_SET          0
#define SEEK_CUR          1

#define RLIMIT_DATA       2
#define RLIMIT_NOFILE     7
#define RLIMIT_AS         9

#define UTS_FIELD         65
#define STAT_SIZE_OFF     48
#define STATX_BYTES       256

static uint64_t g_mmap_bump = MMAP_BUMP_BASE;
static uint64_t g_brk       = BRK_LO;
/* volatile: el tid se guarda aunque nadie lo lea todavía. */
static int     *volatile g_clear_tid;

static void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);

    __asm__ volatile("wrmsr" :: "c"(msr), "a"(lo), "d"(hi) : "memory");
}

static uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo;
    uint32_t hi;

    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static void user_copy_path(char *dst, size_t dst_sz, const char *src)
{
    size_t i;

    if (dst_sz == 0u) {
        return;
    }
    for (i = 0; i + 1u < dst_sz; i++) {
        dst[i] = src[i];
        if (dst[i] == '\0') {
            return;
        }
    }
    dst[dst_sz - 1u] = '\0';
}

static int range_in_user(uint64_t addr, uint64_t length)
{
    if (length == 0u) {
        return 1;
    }
    if (addr < USER_LO || length > USER_HI || addr > USER_HI - length) {
        return 0;
    }
    return 1;
}

static int64_t user_pages_publish(uint64_t dest, uint64_t length)
{
    uint64_t base = dest & ~(PAGE_SIZE - 1ull);
    uint64_t end  = (dest + length + (PAGE_SIZE - 1ull)) & ~(PAGE_SIZE - 1ull);

    (void)pmm_mark_reserved((metta_phys_addr_t)base, (metta_u64)(end - base));
    if (vmm_allow_user((metta_phys_addr_t)dest, (metta_u64)length) != METTA_OK) {
        return -(int64_t)LINUX_ENOMEM;
    }
    return 0;
}

static void put_u16(uint8_t *p, uint16_t v)
{
    memcpy(p, &v, sizeof(v));
}

static void put_u32(uint8_t *p, uint32_t v)
{
    memcpy(p, &v, sizeof(v));
}

static void put_u64(uint8_t *p, uint64_t v)
{
    memcpy(p, &v, sizeof(v));
}

static uint64_t stat_st_size(const void *statbuf)
{
    uint64_t size = 0;

    memcpy(&size, (const uint8_t *)statbuf + STAT_SIZE_OFF, sizeof(size));
    return size;
}

static uint32_t g_trace_n;

void linux_user_trace_reset(void)
{
    g_trace_n = 0;
}

void linux_user_mmap_reset(void)
{
    g_mmap_bump = MMAP_BUMP_BASE;
}

uint64_t linux_user_mmap_end(void)
{
    return g_mmap_bump;
}

void linux_user_trace_result(uint64_t nr, uint64_t a0, int64_t ret)
{
    if (linux_user_trace == 0 || g_trace_n >= 400u) {
        return;
    }
    g_trace_n++;
    KLOG_INFO("sc %llu a0=%llx -> %lld\n",
              (unsigned long long)nr,
              (unsigned long long)a0,
              (long long)ret);
}

int64_t linux_rt_lseek(int fd, int64_t offset, int whence)
{
    if (linux_user_ram_owned(fd)) {
        return linux_user_ram_lseek(fd, offset, whence);
    }
    if (!linux_user_file_owned(fd)) {
        return -(int64_t)LINUX_EBADF;
    }
    return linux_user_file_lseek(fd, offset, whence);
}

int64_t linux_rt_pread64(int fd, void *buf, uint64_t count, int64_t offset)
{
    int64_t cur;
    int64_t pos;
    int64_t n;

    if (!linux_user_file_owned(fd)) {
        return -(int64_t)LINUX_EBADF;
    }
    if (buf == NULL && count != 0u) {
        return -(int64_t)LINUX_EFAULT;
    }

    cur = linux_user_file_lseek(fd, 0, SEEK_CUR);
    if (cur < 0) {
        return cur;
    }
    pos = linux_user_file_lseek(fd, offset, SEEK_SET);
    if (pos < 0) {
        (void)linux_user_file_lseek(fd, cur, SEEK_SET);
        return pos;
    }
    n = linux_user_file_read(fd, buf, count);
    (void)linux_user_file_lseek(fd, cur, SEEK_SET);
    return n;
}

int64_t linux_rt_writev(int fd, const void *iov, int iovcnt)
{
    const struct {
        void    *base;
        uint64_t len;
    } *vec;

    int64_t total = 0;
    int i;

    if (iovcnt < 1) {
        return 0;
    }
    if (iov == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    if (iovcnt > 32) {
        iovcnt = 32;
    }
    vec = iov;
    for (i = 0; i < iovcnt; i++) {
        int64_t n;

        if (vec[i].len == 0u) {
            continue;
        }
        n = linux_rt_write(fd, vec[i].base, vec[i].len);
        if (n < 0) {
            return total > 0 ? total : n;
        }
        total += n;
        if ((uint64_t)n < vec[i].len) {
            break;
        }
    }
    return total;
}

int64_t linux_rt_mmap(void *addr, uint64_t length, int prot, int flags,
                      int fd, int64_t offset)
{
    uint64_t dest;
    int      fixed;
    int      anon;

    (void)prot;
    /* MAP_PRIVATE: se copia el contenido; no hay COW. */
    (void)MAP_PRIVATE;

    if (length == 0u) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (length > ~0ull - (PAGE_SIZE - 1ull)) {
        return -(int64_t)LINUX_ENOMEM;
    }
    length = (length + (PAGE_SIZE - 1ull)) & ~(PAGE_SIZE - 1ull);

    fixed = ((((flags & MAP_FIXED) != 0) || ((flags & MAP_FIXED_NOREPLACE) != 0)) &&
             addr != NULL);
    anon  = ((flags & MAP_ANONYMOUS) != 0) || fd < 0;

    if (!anon && (offset < 0 || ((uint64_t)offset & (PAGE_SIZE - 1ull)) != 0ull)) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (fixed && (((uint64_t)addr & (PAGE_SIZE - 1ull)) != 0ull)) {
        return -(int64_t)LINUX_EINVAL;
    }

    if (fixed) {
        dest = (uint64_t)addr;
        if (dest < USER_LO || length > USER_HI || dest > USER_HI - length) {
            KLOG_INFO("mmap fixed %llx %llx\n",
                      (unsigned long long)dest,
                      (unsigned long long)length);
            return -(int64_t)LINUX_ENOMEM;
        }
    } else {
        if (length > MMAP_BUMP_LIMIT || g_mmap_bump > MMAP_BUMP_LIMIT - length) {
            return -(int64_t)LINUX_ENOMEM;
        }
        dest = g_mmap_bump;
    }

    if (user_pages_publish(dest, length) != 0) {
        return -(int64_t)LINUX_ENOMEM;
    }
    if (!fixed) {
        g_mmap_bump = dest + length;
    }

    memset((void *)dest, 0, (size_t)length);
    if (g_trace_n < 24u) {
        KLOG_INFO("mmap %llx+%llx fl=%x fd=%d off=%llx\n",
                  (unsigned long long)dest,
                  (unsigned long long)length,
                  flags,
                  fd,
                  (unsigned long long)offset);
    }
    if (!anon) {
        uint64_t       sz   = 0;
        const uint8_t *data = linux_user_file_data(fd, &sz);
        uint64_t       off  = (uint64_t)offset;

        if (data != NULL && off < sz) {
            uint64_t n = sz - off;

            if (n > length) {
                n = length;
            }
            memcpy((void *)dest, data + off, (size_t)n);
        } else if (data == NULL && linux_user_file_owned(fd)) {
            (void)linux_user_file_pread(fd, (void *)dest, length, off);
        }
    }
    return (int64_t)dest;
}

int64_t linux_rt_munmap(void *addr, uint64_t length)
{
    uint64_t dest = (uint64_t)addr;

    if (length == 0u) {
        return 0;
    }
    if (!range_in_user(dest, length)) {
        return -(int64_t)LINUX_EINVAL;
    }
    memset(addr, 0, (size_t)length);
    return 0;
}

int64_t linux_rt_mprotect(void *addr, uint64_t len, int prot)
{
    (void)prot;

    if (len == 0u) {
        return 0;
    }
    if (!range_in_user((uint64_t)addr, len)) {
        return -(int64_t)LINUX_ENOMEM;
    }
    return 0;
}

int64_t linux_rt_madvise(void *addr, uint64_t length, int advice)
{
    (void)addr;
    (void)length;
    (void)advice;
    return 0;
}

int64_t linux_rt_brk(void *new_brk)
{
    uint64_t nb = (uint64_t)new_brk;

    if (nb == 0u) {
        return (int64_t)g_brk;
    }
    if (nb < BRK_LO || nb >= BRK_HI) {
        return (int64_t)g_brk;
    }
    if (nb > g_brk) {
        uint64_t gap = nb - g_brk;

        if (user_pages_publish(g_brk, gap) != 0) {
            return (int64_t)g_brk;
        }
        memset((void *)g_brk, 0, (size_t)gap);
    }
    g_brk = nb;
    return (int64_t)g_brk;
}

int64_t linux_rt_ioctl(int fd, uint64_t request, uint64_t arg)
{
    (void)fd;

    if (request == TCGETS) {
        if (arg != 0u) {
            memset((void *)arg, 0, 64);
        }
        return 0;
    }
    if (request == TIOCGWINSZ) {
        uint16_t ws[4];

        if (arg == 0u) {
            return -(int64_t)LINUX_EFAULT;
        }
        ws[0] = 24;
        ws[1] = 80;
        ws[2] = 0;
        ws[3] = 0;
        memcpy((void *)arg, ws, sizeof(ws));
        return 0;
    }
    return 0;
}

int64_t linux_rt_fcntl(int fd, int cmd, uint64_t arg)
{
    (void)fd;
    (void)cmd;
    (void)arg;
    /* F_GETFD (1), F_SETFD (2), F_GETFL (3) y el resto: 0. */
    return 0;
}

int64_t linux_rt_getcwd(char *buf, uint64_t size)
{
    static const char cwd[] = "/home/usuario";

    if (buf == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    if (size < 16u) {
        return -(int64_t)LINUX_EINVAL;
    }
    memcpy(buf, cwd, sizeof(cwd));
    return (int64_t)strlen(cwd);
}

int64_t linux_rt_readlink(const char *path, char *buf, uint64_t bufsiz)
{
    char kpath[256];

    if (path == NULL || buf == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    user_copy_path(kpath, sizeof(kpath), path);
    return linux_user_readlink_path(kpath, buf, bufsiz);
}

int64_t linux_rt_readlinkat(int dirfd, const char *path, char *buf, uint64_t bufsiz)
{
    /* Path absoluto: dirfd no importa. El relativo se entrega tal cual. */
    (void)dirfd;
    return linux_rt_readlink(path, buf, bufsiz);
}

int64_t linux_rt_fstat(int fd, void *statbuf)
{
    uint8_t *st;
    uint32_t mode;

    if (statbuf == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    if (linux_xdisplay_owned(fd)) {
        st = statbuf;
        mode = 0140666u;
        memset(st, 0, 144);
        memcpy(st + 24, &mode, sizeof(mode));
        return 0;
    }
    if (linux_user_ram_owned(fd)) {
        return (int64_t)linux_user_ram_fstat(fd, statbuf);
    }
    return (int64_t)linux_user_file_fstat(fd, statbuf);
}

int64_t linux_rt_newfstatat(int dirfd, const char *path, void *statbuf, int flags)
{
    char kpath[256];
    int  rc;

    (void)dirfd;
    (void)flags;

    if (path == NULL || statbuf == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    user_copy_path(kpath, sizeof(kpath), path);
    if (kpath[0] == '\0') {
        return -(int64_t)LINUX_ENOENT;
    }
    rc = linux_user_file_stat_path(kpath, statbuf);
    if (rc != 0) {
        return rc < 0 ? (int64_t)rc : -(int64_t)LINUX_ENOENT;
    }
    return 0;
}

int64_t linux_rt_access(const char *path, int mode)
{
    uint8_t st[STATX_BYTES];
    char    kpath[256];
    int     rc;

    (void)mode;

    if (path == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    user_copy_path(kpath, sizeof(kpath), path);
    if (kpath[0] == '\0') {
        return -(int64_t)LINUX_ENOENT;
    }
    memset(st, 0, sizeof(st));
    rc = linux_user_file_stat_path(kpath, st);
    if (rc != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    return 0;
}

int64_t linux_rt_faccessat(int dirfd, const char *path, int mode)
{
    (void)dirfd;
    return linux_rt_access(path, mode);
}

int64_t linux_rt_faccessat2(int dirfd, const char *path, int mode, int flags)
{
    (void)flags;
    return linux_rt_faccessat(dirfd, path, mode);
}

int64_t linux_rt_statx(int dirfd, const char *path, int flags, uint32_t mask, void *statxbuf)
{
    uint8_t  st[STATX_BYTES];
    char     kpath[256];
    uint8_t *out;
    uint64_t size;
    int      rc;

    (void)dirfd;
    (void)flags;
    (void)mask;

    if (path == NULL || statxbuf == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    user_copy_path(kpath, sizeof(kpath), path);
    if (kpath[0] == '\0') {
        return -(int64_t)LINUX_ENOENT;
    }
    memset(st, 0, sizeof(st));
    rc = linux_user_file_stat_path(kpath, st);
    if (rc != 0) {
        return -(int64_t)LINUX_ENOENT;
    }

    size = stat_st_size(st);
    out  = (uint8_t *)statxbuf;
    memset(out, 0, STATX_BYTES);
    put_u32(out + 0, 0x7ffu);
    put_u32(out + 4, 4096u);
    put_u64(out + 8, 0);
    put_u32(out + 16, 1u);
    put_u32(out + 20, 0);
    put_u32(out + 24, 0);
    {
        uint32_t st_mode = 0;

        memcpy(&st_mode, st + 24, sizeof(st_mode));
        put_u16(out + 28, (uint16_t)st_mode);
    }
    put_u64(out + 32, 1u);
    put_u64(out + 40, size);
    return 0;
}

int64_t linux_rt_getuid(void)
{
    return 1000;
}

int64_t linux_rt_geteuid(void)
{
    return 1000;
}

int64_t linux_rt_getgid(void)
{
    return 1000;
}

int64_t linux_rt_getegid(void)
{
    return 1000;
}

int64_t linux_rt_arch_prctl(int code, uint64_t addr)
{
    uint64_t val;

    if (code == ARCH_SET_FS) {
        wrmsr(MSR_FS_BASE, addr);
        return 0;
    }
    if (code == ARCH_GET_FS) {
        if (addr == 0u) {
            return -(int64_t)LINUX_EFAULT;
        }
        val = rdmsr(MSR_FS_BASE);
        *(uint64_t *)addr = val;
        return 0;
    }
    return -(int64_t)LINUX_EINVAL;
}

int64_t linux_rt_set_tid_address(int *tidptr)
{
    g_clear_tid = tidptr;
    return 1;
}

int64_t linux_rt_set_robust_list(void *head, uint64_t len)
{
    (void)head;
    (void)len;
    return 0;
}

/* Un solo hilo: despertar a nadie es éxito. Esperar con el valor cambiado es EAGAIN. */
#define FUTEX_WAIT        0
#define FUTEX_WAKE        1
#define FUTEX_REQUEUE     3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP     5
#define FUTEX_LOCK_PI     6
#define FUTEX_UNLOCK_PI   7
#define FUTEX_TRYLOCK_PI  8
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_LOCK_PI2    13

int64_t linux_rt_clone(uint64_t flags, uint64_t stack, uint64_t parent_tid,
                       uint64_t child_tid, uint64_t tls)
{
    int rc = linux_user_clone_enter(stack, flags, parent_tid, child_tid, tls);

    return (int64_t)rc;
}

int64_t linux_rt_futex(uint64_t uaddr, int op, int val, uint64_t timeout,
                       uint64_t uaddr2, int val3)
{
    int cmd = op & ~(128 | 256);
    int32_t cur;
    static int logged;

    (void)timeout;
    (void)uaddr2;
    (void)val3;
    if (logged < 32) {
        logged++;
        KLOG_INFO("futex op=%d val=%d uaddr=%llx\n",
                  op, val, (unsigned long long)uaddr);
    }
    if (uaddr < USER_LO || uaddr > USER_HI - 4u || (uaddr & 3u) != 0u) {
        return -(int64_t) LINUX_EFAULT;
    }
    switch (cmd) {
    case FUTEX_WAIT:
    case FUTEX_WAIT_BITSET:
        cur = *(volatile int32_t *)(uintptr_t)uaddr;
        if (cur != val) {
            return -(int64_t) LINUX_EAGAIN;
        }
        {
            int64_t handed;

            /* El hijo no puede dormir de verdad: si lo hace, el padre no corre. */
            if (linux_user_futex_block(uaddr, &handed)) {
                return handed;
            }
        }
        return 0;
    case FUTEX_WAKE:
    case FUTEX_WAKE_BITSET:
        {
            int64_t handed;

            if (linux_user_futex_wake(uaddr, &handed)) {
                return handed;
            }
        }
        return 0;
    case FUTEX_REQUEUE:
    case FUTEX_CMP_REQUEUE:
    case FUTEX_WAKE_OP:
    case FUTEX_LOCK_PI:
    case FUTEX_UNLOCK_PI:
    case FUTEX_TRYLOCK_PI:
    case FUTEX_LOCK_PI2:
        return 0;
    default:
        return -(int64_t) LINUX_ENOSYS;
    }
}

int64_t linux_rt_prlimit64(int pid, int resource, const void *new_limit, void *old_limit)
{
    uint64_t cur;
    uint64_t max;
    uint64_t pair[2];

    (void)pid;
    (void)new_limit;

    if (old_limit == NULL) {
        return 0;
    }
    if (resource == RLIMIT_NOFILE) {
        cur = 1024u;
        max = 1024u;
    } else if (resource == RLIMIT_DATA || resource == RLIMIT_AS) {
        cur = 0x7FFFFFFFFFFFFFFFull;
        max = 0x7FFFFFFFFFFFFFFFull;
    } else {
        /* Incluye RLIMIT_STACK (3): 8 MiB. */
        cur = 8ull * 1024ull * 1024ull;
        max = cur;
    }
    pair[0] = cur;
    pair[1] = max;
    memcpy(old_limit, pair, sizeof(pair));
    return 0;
}

int64_t linux_rt_getrandom(void *buf, uint64_t buflen, uint32_t flags)
{
    uint64_t addr = (uint64_t)buf;

    (void)flags;

    if (buf == NULL || addr < USER_LO || addr >= USER_HI || buflen > (1ull << 20) ||
        buflen > USER_HI || addr > USER_HI - buflen) {
        return -(int64_t)LINUX_EFAULT;
    }
    memset(buf, 0xA5, (size_t)buflen);
    return (int64_t)buflen;
}

int64_t linux_rt_clock_gettime(int clk_id, void *tp)
{
    uint64_t ticks;
    int64_t  ts[2];

    (void)clk_id;

    if (tp == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    ticks = pit_ticks_get();
    ts[0] = (int64_t)(ticks / 100u);
    ts[1] = (int64_t)((ticks % 100u) * 10000000u);
    memcpy(tp, ts, sizeof(ts));
    return 0;
}

int64_t linux_rt_uname(void *buf)
{
    char *b;

    if (buf == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    memset(buf, 0, (size_t)(UTS_FIELD * 6));
    b = (char *)buf;
    memcpy(b + (UTS_FIELD * 0), "Linux", 6);
    memcpy(b + (UTS_FIELD * 1), "metta", 6);
    memcpy(b + (UTS_FIELD * 2), "6.8.0", 6);
    memcpy(b + (UTS_FIELD * 3), "#1 SMP", 7);
    memcpy(b + (UTS_FIELD * 4), "x86_64", 7);
    memcpy(b + (UTS_FIELD * 5), "localdomain", 12);
    return 0;
}

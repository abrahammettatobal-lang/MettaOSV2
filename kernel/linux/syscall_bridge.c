/*
 * Puente linux_rt_* → runtime/señales SA-32.
 * Gaps sin símbolo en SA-32: stubs débiles → -ENOSYS.
 */
#include "syscall_linux.h"
#include <linux/usermode.h>
#include <linux/user_files.h>
#include <linux/user_ram.h>
#include <linux/xdisplay.h>

#include <log/klog.h>
#include <log/printk.h>

#include <linux/runtime.h>
#include <linux/signal.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LINUX_RT_ENOSYS() return -(int64_t)LINUX_ENOSYS

static int64_t linux_rt_metta_status(metta_status_t st)
{
    switch (st) {
    case METTA_OK:
        return 0;
    case METTA_ERR_NOT_FOUND:
        return -(int64_t)LINUX_EBADF;
    case METTA_ERR_INVALID_ARG:
    case METTA_ERR_RANGE:
        return -(int64_t)LINUX_EINVAL;
    case METTA_ERR_NO_MEMORY:
        return -(int64_t)LINUX_ENOMEM;
    case METTA_ERR_BUSY:
        return -(int64_t)LINUX_EBUSY;
    case METTA_ERR_PERM:
        return -(int64_t)LINUX_EACCES;
    default:
        return -(int64_t)LINUX_EINVAL;
    }
}

static linux_proc_t *linux_rt_proc(void)
{
    if (!linux_runtime_ready()) {
        if (linux_runtime_init() != METTA_OK) {
            return NULL;
        }
    }
    return linux_proc_current();
}

/* --- Implementados vía SA-32 (símbolos fuertes) --- */

int64_t linux_rt_read(int fd, void *buf, uint64_t count)
{
    linux_proc_t    *proc = linux_rt_proc();
    linux_fd_kind_t  kind;

    if (proc == NULL || buf == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    if (fd < 0 || (linux_fd_t)fd >= METTA_LINUX_MAX_FDS) {
        return -(int64_t)LINUX_EBADF;
    }

    if (linux_xdisplay_owned(fd)) {
        return linux_xdisplay_read(fd, buf, count);
    }
    if (linux_user_ram_owned(fd)) {
        return linux_user_ram_read(fd, buf, count);
    }

    kind = linux_fd_kind(proc, (linux_fd_t)fd);
    switch (kind) {
    case LINUX_FD_PIPE_RD:
        return (int64_t)linux_pipe_read(proc, (linux_fd_t)fd, buf, count);
    case LINUX_FD_X11:
        return -(int64_t)107;
    case LINUX_FD_TTY_STUB:
        if (count == 0) {
            return 0;
        }
        return 0;
    case LINUX_FD_FILE_STUB:
        if (linux_user_file_owned(fd)) {
            return linux_user_file_read(fd, buf, count);
        }
        LINUX_RT_ENOSYS();
    default:
        if (kind == LINUX_FD_NONE) {
            return -(int64_t)LINUX_EBADF;
        }
        LINUX_RT_ENOSYS();
    }
}

int64_t linux_rt_write(int fd, const void *buf, uint64_t count)
{
    linux_proc_t    *proc = linux_rt_proc();
    linux_fd_kind_t  kind;

    if (proc == NULL || buf == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    if (fd < 0 || (linux_fd_t)fd >= METTA_LINUX_MAX_FDS) {
        return -(int64_t)LINUX_EBADF;
    }

    if (linux_xdisplay_owned(fd)) {
        return linux_xdisplay_write(fd, buf, count);
    }
    if (linux_user_ram_owned(fd)) {
        return linux_user_ram_write(fd, buf, count);
    }

    kind = linux_fd_kind(proc, (linux_fd_t)fd);
    switch (kind) {
    case LINUX_FD_PIPE_WR:
        return (int64_t)linux_pipe_write(proc, (linux_fd_t)fd, buf, count);
    case LINUX_FD_X11:
        return -(int64_t)107;
    case LINUX_FD_TTY_STUB: {
        const uint8_t *bytes = (const uint8_t *)buf;
        uint64_t       left  = count;

        while (left > 0u) {
            char     tmp[128];
            uint64_t n = left > 127u ? 127u : left;
            uint64_t i;

            for (i = 0; i < n; i++) {
                tmp[i] = (char)bytes[i];
            }
            tmp[n] = '\0';
            printk("%s", tmp);
            bytes += n;
            left  -= n;
        }
        return (int64_t)count;
    }
    case LINUX_FD_FILE_STUB:
    default:
        if (kind == LINUX_FD_NONE) {
            return -(int64_t)LINUX_EBADF;
        }
        LINUX_RT_ENOSYS();
    }
}

/* Copia el path de usuario (mapa identidad) a un buffer del kernel. */
static void linux_rt_copy_path(char *dst, size_t dst_sz, const char *src)
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

int64_t linux_rt_open(const char *path, int flags, int mode)
{
    linux_proc_t *proc;
    char          kpath[256];
    int           idx;
    metta_s32     fd;
    static int    log_ok;
    static int    log_miss;

    if (path == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }

    linux_rt_copy_path(kpath, sizeof(kpath), path);
    (void)linux_user_files_init();
    linux_user_ram_init();

    idx = linux_user_file_find(kpath);
    if (idx < 0) {
        uint8_t  probe[144];
        uint32_t st_mode = 0;
        int      created;

        if (linux_user_ram_stat(kpath, probe) == 0) {
            memcpy(&st_mode, probe + 24, sizeof(st_mode));
            if ((st_mode & 0170000u) == 0040000u) {
                return -(int64_t)LINUX_EISDIR;
            }
        } else if ((flags & 0100) == 0) {
            if (log_miss < 200) {
                log_miss++;
                KLOG_INFO("open %s -> ENOENT\n", kpath);
            }
            return -(int64_t) LINUX_ENOENT;
        }

        proc = linux_rt_proc();
        if (proc == NULL) {
            LINUX_RT_ENOSYS();
        }
        fd = linux_fd_open_stub(proc, LINUX_FD_FILE_STUB);
        if (fd < 0) {
            return (int64_t)fd;
        }
        created = linux_user_ram_create((int)fd, kpath, (flags & 01000) != 0);
        if (created != 0) {
            (void)linux_fd_close(proc, (linux_fd_t)fd);
            return (int64_t)created;
        }
        if (log_ok < 200) {
            log_ok++;
            KLOG_INFO("open %s -> %d\n", kpath, (int)fd);
        }
        return (int64_t)fd;
    }
    (void)mode;

    proc = linux_rt_proc();
    if (proc == NULL) {
        LINUX_RT_ENOSYS();
    }

    fd = linux_fd_open_stub(proc, LINUX_FD_FILE_STUB);
    if (fd < 0) {
        return (int64_t)fd;
    }

    linux_user_file_bind((int)fd, idx);
    if (log_ok < 200) {
        log_ok++;
        KLOG_INFO("open %s -> %d\n", kpath, (int)fd);
    }
    return (int64_t)fd;
}

int64_t linux_rt_openat(int dirfd, const char *path, int flags, int mode)
{
    /* AT_FDCWD vale -100. Un path absoluto ignora dirfd; open resuelve el resto. */
    (void)dirfd;
    return linux_rt_open(path, flags, mode);
}

int64_t linux_rt_close(int fd)
{
    linux_proc_t  *proc = linux_rt_proc();
    metta_status_t st;

    if (proc == NULL) {
        LINUX_RT_ENOSYS();
    }
    if (fd < 0) {
        return -(int64_t)LINUX_EBADF;
    }

    if (linux_xdisplay_owned(fd)) {
        linux_xdisplay_attach(-1);
    }
    linux_user_ram_close(fd);
    linux_user_file_detach(fd);
    st = linux_fd_close(proc, (linux_fd_t)fd);
    return linux_rt_metta_status(st);
}

int64_t linux_rt_pipe(int pipefd[2])
{
    linux_proc_t *proc = linux_rt_proc();
    metta_s32     rc;

    if (pipefd == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    if (proc == NULL) {
        LINUX_RT_ENOSYS();
    }

    rc = linux_pipe_create(proc, (linux_fd_t *)pipefd);
    return (int64_t)rc;
}

int64_t linux_rt_socket(int domain, int type, int protocol)
{
    linux_proc_t *proc;
    metta_s32     fd;

    (void)protocol;
    if (domain != 1 || (type & 0xf) != 1) {
        return -(int64_t)LINUX_EAFNOSUPPORT;
    }
    proc = linux_rt_proc();
    if (proc == NULL) {
        LINUX_RT_ENOSYS();
    }
    fd = linux_fd_open_stub(proc, LINUX_FD_X11);
    if (fd >= 0) {
        KLOG_INFO("x11 socket %d\n", (int)fd);
    }
    return (int64_t)fd;
}

int64_t linux_rt_connect(int fd, const void *addr, uint64_t addrlen)
{
    if (addr == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    if (linux_fd_kind(linux_rt_proc(), (linux_fd_t)fd) != LINUX_FD_X11) {
        return -(int64_t)LINUX_EBADF;
    }
    if (!linux_xdisplay_is_path(addr, addrlen)) {
        return -(int64_t)111;
    }
    linux_xdisplay_attach(fd);
    KLOG_INFO("x11 connect %d\n", fd);
    return 0;
}

int64_t linux_rt_getsockname(int fd, void *addr, void *addrlen)
{
    uint8_t *out = addr;
    uint32_t have;
    uint32_t need;
    const char path[] = "/tmp/.X11-unix/X0";
    uint32_t i;

    if (!linux_xdisplay_owned(fd)) {
        return -(int64_t)LINUX_EBADF;
    }
    if (addr == NULL || addrlen == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    memcpy(&have, addrlen, sizeof(have));
    need = 2u + (uint32_t)sizeof(path);
    if (have >= 2u) {
        out[0] = 1;
        out[1] = 0;
    }
    for (i = 0; i + 2u < have && path[i] != '\0'; i++) {
        out[2u + i] = (uint8_t)path[i];
    }
    if (2u + i < have) {
        out[2u + i] = 0;
    }
    memcpy(addrlen, &need, sizeof(need));
    return 0;
}

int64_t linux_rt_poll(void *fds, uint64_t nfds, int timeout)
{
    uint8_t *p = fds;
    linux_proc_t *proc;
    uint64_t i;
    int ready = 0;

    (void)timeout;
    if (nfds == 0u) {
        return 0;
    }
    if (fds == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    if (nfds > 256u) {
        nfds = 256u;
    }
    proc = linux_rt_proc();
    for (i = 0; i < nfds; i++) {
        uint8_t *slot = p + i * 8u;
        int fd;
        short events;
        short re = 0;
        linux_fd_kind_t kind;

        memcpy(&fd, slot, sizeof(fd));
        memcpy(&events, slot + 4, sizeof(events));
        kind = (proc != NULL) ? linux_fd_kind(proc, (linux_fd_t)fd) : LINUX_FD_NONE;
        if (fd < 0 || (kind == LINUX_FD_NONE && !linux_xdisplay_owned(fd))) {
            re = (short)0x20;
        } else {
            if ((events & 1) != 0 &&
                (linux_xdisplay_poll_in(fd) || linux_pipe_pending(proc, (linux_fd_t)fd))) {
                re = (short)(re | 1);
            }
            if ((events & 4) != 0) {
                re = (short)(re | 4);
            }
        }
        memcpy(slot + 6, &re, sizeof(re));
        if (re != 0) {
            ready++;
        }
    }
    return ready;
}

int64_t linux_rt_ppoll(void *fds, uint64_t nfds, const void *timeout, const void *sigmask)
{
    (void)timeout;
    (void)sigmask;
    return linux_rt_poll(fds, nfds, 0);
}

int64_t linux_rt_mkdir(const char *path, int mode)
{
    char kpath[256];

    (void)mode;
    if (path == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    linux_user_ram_init();
    linux_rt_copy_path(kpath, sizeof(kpath), path);
    return (int64_t)linux_user_ram_mkdir(kpath);
}

int64_t linux_rt_pipe2(int pipefd[2], int flags)
{
    /* O_CLOEXEC y O_NONBLOCK: la lectura vacía ya devuelve EAGAIN. */
    (void)flags;
    return linux_rt_pipe(pipefd);
}

/* AF_UNIX / SOCK_SEQPACKET del crash reporter: un pipe en memoria basta. */
int64_t linux_rt_socketpair(int domain, int type, int protocol, int sv[2])
{
    int64_t rc;

    (void)domain;
    (void)type;
    (void)protocol;
    rc = linux_rt_pipe(sv);
    if (rc == 0) {
        KLOG_INFO("socketpair -> %d,%d\n", sv[0], sv[1]);
    }
    return rc;
}

int64_t linux_rt_setsockopt(int fd, int level, int optname, const void *optval,
                            uint64_t optlen)
{
    (void)fd;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return 0;
}

int64_t linux_rt_getsockopt(int fd, int level, int optname, void *optval, void *optlen)
{
    (void)fd;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return 0;
}

int64_t linux_rt_sendto(int fd, const void *buf, uint64_t len, int flags,
                        const void *dest, uint64_t addrlen)
{
    (void)flags;
    (void)dest;
    (void)addrlen;
    return linux_rt_write(fd, buf, len);
}

int64_t linux_rt_recvfrom(int fd, void *buf, uint64_t len, int flags,
                          void *src, void *addrlen)
{
    (void)flags;
    (void)src;
    (void)addrlen;
    return linux_rt_read(fd, buf, len);
}

int64_t linux_rt_sendmsg(int fd, const void *msg, int flags)
{
    const uint8_t *bytes;
    void          *iov;
    void          *base;
    uint64_t       len;

    (void)flags;
    if (msg == NULL) {
        return -(int64_t)LINUX_EFAULT;
    }
    bytes = (const uint8_t *)msg;
    memcpy(&iov, bytes + 16, sizeof(iov));
    if (iov == NULL) {
        return 0;
    }
    memcpy(&base, iov, sizeof(base));
    memcpy(&len, (const uint8_t *)iov + 8, sizeof(len));
    return linux_rt_write(fd, base, len);
}

int64_t linux_rt_recvmsg(int fd, void *msg, int flags)
{
    uint8_t *bytes;
    void    *iov;
    void    *base;
    uint64_t len;

    (void)flags;
    if (msg == NULL) {
        return -(int64_t) LINUX_EFAULT;
    }
    bytes = (uint8_t *)msg;
    memcpy(&iov, bytes + 16, sizeof(iov));
    if (iov == NULL) {
        return 0;
    }
    memcpy(&base, iov, sizeof(base));
    memcpy(&len, (const uint8_t *)iov + 8, sizeof(len));
    return linux_rt_read(fd, base, len);
}

int64_t linux_rt_shutdown(int fd, int how)
{
    (void)fd;
    (void)how;
    return 0;
}

int64_t linux_rt_getpid(void)
{
    linux_proc_t *proc = linux_rt_proc();

    if (proc == NULL) {
        LINUX_RT_ENOSYS();
    }
    return (int64_t)linux_proc_pid(proc);
}

int64_t linux_rt_gettid(void)
{
    return linux_user_current_tid();
}

int64_t linux_rt_exit(int status)
{
    linux_proc_t  *proc = linux_rt_proc();
    metta_status_t st;
    int64_t        tid;

    if (linux_user_clone_exit(&tid)) {
        return tid;
    }

    if (proc == NULL) {
        LINUX_RT_ENOSYS();
    }

    st = linux_proc_mark_zombie(proc, (metta_s32)status);
    if (st != METTA_OK) {
        return linux_rt_metta_status(st);
    }
    linux_user_note_exit(status);
    return 0;
}

int64_t linux_rt_exit_group(int status)
{
    linux_user_clone_cancel();
    return linux_rt_exit(status);
}

int64_t linux_rt_rt_sigaction(int sig, const void *act, void *oact, uint64_t sigsetsize)
{
    linux_proc_t *proc = linux_rt_proc();
    metta_s32     rc;

    if (proc == NULL) {
        LINUX_RT_ENOSYS();
    }
    if (sigsetsize != sizeof(linux_sigset_t)) {
        return -(int64_t)LINUX_EINVAL;
    }

    rc = linux_sigaction(proc, sig,
                         (const linux_sigaction_t *)act,
                         (linux_sigaction_t *)oact);
    return (int64_t)rc;
}

int64_t linux_rt_rt_sigprocmask(int how, const void *set, void *oldset, uint64_t sigsetsize)
{
    linux_proc_t *proc = linux_rt_proc();
    metta_s32     rc;

    if (proc == NULL) {
        LINUX_RT_ENOSYS();
    }
    if (sigsetsize != sizeof(linux_sigset_t)) {
        return -(int64_t)LINUX_EINVAL;
    }

    rc = linux_sigprocmask(proc, how,
                           (const linux_sigset_t *)set,
                           (linux_sigset_t *)oldset);
    return (int64_t)rc;
}

/*
 * Gaps sin símbolo fuerte: stubs débiles → -ENOSYS.
 * lseek, mmap, brk, ioctl, fcntl, credenciales y uname viven en user_syscall.c.
 */

__attribute__((weak)) int64_t linux_rt_dup(int oldfd)
{
    (void)oldfd;
    LINUX_RT_ENOSYS();
}

__attribute__((weak)) int64_t linux_rt_dup2(int oldfd, int newfd)
{
    (void)oldfd;
    (void)newfd;
    LINUX_RT_ENOSYS();
}

__attribute__((weak)) int64_t linux_rt_getdents64(int fd, void *dirp, uint64_t count)
{
    (void)fd;
    (void)dirp;
    (void)count;
    LINUX_RT_ENOSYS();
}

__attribute__((weak)) int64_t linux_rt_chdir(const char *path)
{
    (void)path;
    LINUX_RT_ENOSYS();
}

__attribute__((weak)) int64_t linux_rt_unlink(const char *path)
{
    (void)path;
    LINUX_RT_ENOSYS();
}

__attribute__((weak)) int64_t linux_rt_rt_sigreturn(void *uc)
{
    (void)uc;
    LINUX_RT_ENOSYS();
}

__attribute__((weak)) int64_t linux_rt_nanosleep(const void *req, void *rem)
{
    (void)req;
    (void)rem;
    LINUX_RT_ENOSYS();
}

#undef LINUX_RT_ENOSYS
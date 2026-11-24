#ifndef METTA_LINUX_SYSCALL_LINUX_H
#define METTA_LINUX_SYSCALL_LINUX_H

#include <stddef.h>
#include <stdint.h>

#include "syscall_errno.h"
#include "syscall_numbers.h"

/*
 * Marco de entrada trap x86_64 (convención Linux):
 *   rax = nr, rdi, rsi, rdx, r10, r8, r9 = args 1..6
 */
typedef struct linux_syscall_frame {
    uint64_t nr;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r10;
    uint64_t r8;
    uint64_t r9;
} linux_syscall_frame_t;

typedef int64_t (*linux_syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                      uint64_t, uint64_t, uint64_t);

void linux_syscall_init(void);

int64_t linux_syscall_dispatch(const linux_syscall_frame_t *frame);

int64_t linux_syscall_dispatch_raw(uint64_t nr, uint64_t rdi, uint64_t rsi,
                                     uint64_t rdx, uint64_t r10, uint64_t r8,
                                     uint64_t r9);

const linux_syscall_fn_t *linux_syscall_table_ref(size_t *count_out);

uint64_t linux_syscall_enosys_count(void);

/* Conversión metta_status_t (kernel nativo) → errno Linux positivo. */
int linux_errno_from_metta(int metta_status);

/*
 * Puente hacia kernel/linux/runtime.c (SA-32) y user_syscall.c.
 * Los que siguen débiles en syscall_bridge.c (dup, nanosleep, …) devuelven -ENOSYS.
 */
int64_t linux_rt_read(int fd, void *buf, uint64_t count);
int64_t linux_rt_write(int fd, const void *buf, uint64_t count);
int64_t linux_rt_open(const char *path, int flags, int mode);
int64_t linux_rt_openat(int dirfd, const char *path, int flags, int mode);
int64_t linux_rt_close(int fd);
int64_t linux_rt_lseek(int fd, int64_t offset, int whence);
int64_t linux_rt_mmap(void *addr, uint64_t length, int prot, int flags,
                      int fd, int64_t offset);
int64_t linux_rt_munmap(void *addr, uint64_t length);
int64_t linux_rt_mprotect(void *addr, uint64_t len, int prot);
int64_t linux_rt_brk(void *new_brk);
int64_t linux_rt_ioctl(int fd, uint64_t request, uint64_t arg);
int64_t linux_rt_fcntl(int fd, int cmd, uint64_t arg);
int64_t linux_rt_pipe(int pipefd[2]);
int64_t linux_rt_pipe2(int pipefd[2], int flags);
int64_t linux_rt_socket(int domain, int type, int protocol);
int64_t linux_rt_connect(int fd, const void *addr, uint64_t addrlen);
int64_t linux_rt_getsockname(int fd, void *addr, void *addrlen);
int64_t linux_rt_poll(void *fds, uint64_t nfds, int timeout);
int64_t linux_rt_ppoll(void *fds, uint64_t nfds, const void *timeout, const void *sigmask);
int64_t linux_rt_mkdir(const char *path, int mode);
int64_t linux_rt_socketpair(int domain, int type, int protocol, int sv[2]);
int64_t linux_rt_setsockopt(int fd, int level, int optname, const void *optval,
                            uint64_t optlen);
int64_t linux_rt_getsockopt(int fd, int level, int optname, void *optval, void *optlen);
int64_t linux_rt_sendto(int fd, const void *buf, uint64_t len, int flags,
                        const void *dest, uint64_t addrlen);
int64_t linux_rt_recvfrom(int fd, void *buf, uint64_t len, int flags,
                          void *src, void *addrlen);
int64_t linux_rt_sendmsg(int fd, const void *msg, int flags);
int64_t linux_rt_recvmsg(int fd, void *msg, int flags);
int64_t linux_rt_shutdown(int fd, int how);
int64_t linux_rt_dup(int oldfd);
int64_t linux_rt_dup2(int oldfd, int newfd);
int64_t linux_rt_getdents64(int fd, void *dirp, uint64_t count);
int64_t linux_rt_getcwd(char *buf, uint64_t size);
int64_t linux_rt_chdir(const char *path);
int64_t linux_rt_unlink(const char *path);
int64_t linux_rt_readlink(const char *path, char *buf, uint64_t bufsiz);
int64_t linux_rt_fstat(int fd, void *statbuf);
int64_t linux_rt_newfstatat(int dirfd, const char *path, void *statbuf, int flags);

int64_t linux_rt_exit(int status);
int64_t linux_rt_exit_group(int status);
int64_t linux_rt_getpid(void);
int64_t linux_rt_gettid(void);
int64_t linux_rt_getuid(void);
int64_t linux_rt_geteuid(void);
int64_t linux_rt_getgid(void);
int64_t linux_rt_getegid(void);

int64_t linux_rt_rt_sigaction(int sig, const void *act, void *oact, uint64_t sigsetsize);
int64_t linux_rt_rt_sigprocmask(int how, const void *set, void *oldset, uint64_t sigsetsize);
int64_t linux_rt_rt_sigreturn(void *uc);
int64_t linux_rt_arch_prctl(int code, uint64_t addr);
int64_t linux_rt_set_tid_address(int *tidptr);
int64_t linux_rt_clock_gettime(int clk_id, void *tp);
int64_t linux_rt_nanosleep(const void *req, void *rem);
int64_t linux_rt_uname(void *buf);

/* Arranque de ld-linux / libc. Números en syscall_numbers.h. rseq (334) sigue en ENOSYS. */
int64_t linux_rt_pread64(int fd, void *buf, uint64_t count, int64_t offset);
int64_t linux_rt_writev(int fd, const void *iov, int iovcnt);
int64_t linux_rt_access(const char *path, int mode);
int64_t linux_rt_madvise(void *addr, uint64_t length, int advice);
int64_t linux_rt_readlinkat(int dirfd, const char *path, char *buf, uint64_t bufsiz);
int64_t linux_rt_faccessat(int dirfd, const char *path, int mode);
int64_t linux_rt_faccessat2(int dirfd, const char *path, int mode, int flags);
int64_t linux_rt_set_robust_list(void *head, uint64_t len);
int64_t linux_rt_futex(uint64_t uaddr, int op, int val, uint64_t timeout,
                       uint64_t uaddr2, int val3);
int64_t linux_rt_clone(uint64_t flags, uint64_t stack, uint64_t parent_tid,
                       uint64_t child_tid, uint64_t tls);
int64_t linux_rt_prlimit64(int pid, int resource, const void *new_limit, void *old_limit);
int64_t linux_rt_getrandom(void *buf, uint64_t buflen, uint32_t flags);
int64_t linux_rt_statx(int dirfd, const char *path, int flags, uint32_t mask, void *statxbuf);

#endif /* METTA_LINUX_SYSCALL_LINUX_H */
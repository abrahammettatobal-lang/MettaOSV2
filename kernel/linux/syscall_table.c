/*
 * Tabla de traducción NR → manejador → servicios linux_rt_* (runtime SA-32).
 */
#include "syscall_linux.h"

#include <stddef.h>

static int64_t sys_read(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                        uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_read((int)a0, (void *)a1, a2);
}

static int64_t sys_write(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_write((int)a0, (const void *)a1, a2);
}

static int64_t sys_open(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                        uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_open((const char *)a0, (int)a1, (int)a2);
}

static int64_t sys_close(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_close((int)a0);
}

static int64_t sys_fstat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_fstat((int)a0, (void *)a1);
}

static int64_t sys_lseek(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_lseek((int)a0, (int64_t)a1, (int)a2);
}

static int64_t sys_mmap(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                        uint64_t a5)
{
    return linux_rt_mmap((void *)a0, a1, (int)a2, (int)a3, (int)a4, (int64_t)a5);
}

static int64_t sys_mprotect(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                            uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_mprotect((void *)a0, a1, (int)a2);
}

static int64_t sys_munmap(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_munmap((void *)a0, a1);
}

static int64_t sys_brk(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                       uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_brk((void *)a0);
}

static int64_t sys_rt_sigaction(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                                uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_rt_sigaction((int)a0, (const void *)a1, (void *)a2, a3);
}

static int64_t sys_rt_sigprocmask(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_rt_sigprocmask((int)a0, (const void *)a1, (void *)a2, a3);
}

static int64_t sys_rt_sigreturn(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                                uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_rt_sigreturn((void *)a0);
}

static int64_t sys_ioctl(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_ioctl((int)a0, a1, a2);
}

static int64_t sys_pipe(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                        uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_pipe((int *)a0);
}

static int64_t sys_pipe2(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_pipe2((int *)a0, (int)a1);
}

static int64_t sys_socketpair(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_socketpair((int)a0, (int)a1, (int)a2, (int *)a3);
}

static int64_t sys_setsockopt(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a5;
    return linux_rt_setsockopt((int)a0, (int)a1, (int)a2, (const void *)a3, a4);
}

static int64_t sys_getsockopt(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a5;
    return linux_rt_getsockopt((int)a0, (int)a1, (int)a2, (void *)a3, (void *)a4);
}

static int64_t sys_sendto(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    return linux_rt_sendto((int)a0, (const void *)a1, a2, (int)a3, (const void *)a4, a5);
}

static int64_t sys_recvfrom(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                            uint64_t a5)
{
    return linux_rt_recvfrom((int)a0, (void *)a1, a2, (int)a3, (void *)a4, (void *)a5);
}

static int64_t sys_sendmsg(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_sendmsg((int)a0, (const void *)a1, (int)a2);
}

static int64_t sys_recvmsg(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_recvmsg((int)a0, (void *)a1, (int)a2);
}

static int64_t sys_shutdown(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                            uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_shutdown((int)a0, (int)a1);
}

static int64_t sys_dup(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                       uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_dup((int)a0);
}

static int64_t sys_dup2(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                        uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_dup2((int)a0, (int)a1);
}

static int64_t sys_nanosleep(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                             uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_nanosleep((const void *)a0, (void *)a1);
}

static int64_t sys_getpid(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_getpid();
}

static int64_t sys_fcntl(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_fcntl((int)a0, (int)a1, a2);
}

static int64_t sys_getdents64(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_getdents64((int)a0, (void *)a1, a2);
}

static int64_t sys_getcwd(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_getcwd((char *)a0, a1);
}

static int64_t sys_chdir(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_chdir((const char *)a0);
}

static int64_t sys_unlink(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_unlink((const char *)a0);
}

static int64_t sys_readlink(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                            uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_readlink((const char *)a0, (char *)a1, a2);
}

static int64_t sys_uname(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_uname((void *)a0);
}

static int64_t sys_getuid(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_getuid();
}

static int64_t sys_getgid(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_getgid();
}

static int64_t sys_geteuid(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_geteuid();
}

static int64_t sys_getegid(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_getegid();
}

static int64_t sys_arch_prctl(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_arch_prctl((int)a0, a1);
}

static int64_t sys_gettid(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_gettid();
}

static int64_t sys_set_tid_address(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                                   uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_set_tid_address((int *)a0);
}

static int64_t sys_clock_gettime(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                                 uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_clock_gettime((int)a0, (void *)a1);
}

static int64_t sys_exit(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                        uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_exit((int)a0);
}

static int64_t sys_exit_group(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_exit_group((int)a0);
}

static int64_t sys_openat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_openat((int)a0, (const char *)a1, (int)a2, (int)a3);
}

static int64_t sys_newfstatat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_newfstatat((int)a0, (const char *)a1, (void *)a2, (int)a3);
}

static int64_t sys_pread64(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_pread64((int)a0, (void *)a1, a2, (int64_t)a3);
}

static int64_t sys_writev(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_writev((int)a0, (const void *)a1, (int)a2);
}

static int64_t sys_access(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_access((const char *)a0, (int)a1);
}

static int64_t sys_madvise(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_madvise((void *)a0, a1, (int)a2);
}

static int64_t sys_readlinkat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_readlinkat((int)a0, (const char *)a1, (char *)a2, a3);
}

static int64_t sys_faccessat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                             uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_faccessat((int)a0, (const char *)a1, (int)a2);
}

static int64_t sys_faccessat2(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                              uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_faccessat2((int)a0, (const char *)a1, (int)a2, (int)a3);
}

static int64_t sys_set_robust_list(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                                   uint64_t a4, uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_set_robust_list((void *)a0, a1);
}

static int64_t sys_futex(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5)
{
    return linux_rt_futex(a0, (int)a1, (int)a2, a3, a4, (int)a5);
}

static int64_t sys_clone(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5)
{
    (void)a5;
    return linux_rt_clone(a0, a1, a2, a3, a4);
}

static int64_t sys_prlimit64(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                             uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_prlimit64((int)a0, (int)a1, (const void *)a2, (void *)a3);
}

static int64_t sys_getrandom(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                             uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_getrandom((void *)a0, a1, (uint32_t)a2);
}

static int64_t sys_statx(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a5;
    return linux_rt_statx((int)a0, (const char *)a1, (int)a2, (uint32_t)a3, (void *)a4);
}

static int64_t sys_poll(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                        uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_poll((void *)a0, a1, (int)a2);
}

static int64_t sys_socket(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_socket((int)a0, (int)a1, (int)a2);
}

static int64_t sys_connect(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_connect((int)a0, (const void *)a1, a2);
}

static int64_t sys_getsockname(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                               uint64_t a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_getsockname((int)a0, (void *)a1, (void *)a2);
}

static int64_t sys_mkdir(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_mkdir((const char *)a0, (int)a1);
}

static int64_t sys_mkdirat(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5)
{
    (void)a0;
    (void)a3;
    (void)a4;
    (void)a5;
    return linux_rt_mkdir((const char *)a1, (int)a2);
}

static int64_t sys_ppoll(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                         uint64_t a5)
{
    (void)a4;
    (void)a5;
    return linux_rt_ppoll((void *)a0, a1, (const void *)a2, (const void *)a3);
}

static const linux_syscall_fn_t linux_syscall_table[LINUX_SYSCALL_MAX] = {
    [__NR_read]            = sys_read,
    [__NR_poll]            = sys_poll,
    [__NR_write]           = sys_write,
    [__NR_open]            = sys_open,
    [__NR_close]           = sys_close,
    [__NR_fstat]           = sys_fstat,
    [__NR_lseek]           = sys_lseek,
    [__NR_mmap]            = sys_mmap,
    [__NR_mprotect]        = sys_mprotect,
    [__NR_munmap]          = sys_munmap,
    [__NR_brk]             = sys_brk,
    [__NR_rt_sigaction]    = sys_rt_sigaction,
    [__NR_rt_sigprocmask]  = sys_rt_sigprocmask,
    [__NR_rt_sigreturn]    = sys_rt_sigreturn,
    [__NR_ioctl]           = sys_ioctl,
    [__NR_pipe]            = sys_pipe,
    [__NR_pipe2]           = sys_pipe2,
    [__NR_socket]          = sys_socket,
    [__NR_connect]         = sys_connect,
    [__NR_sendto]          = sys_sendto,
    [__NR_recvfrom]        = sys_recvfrom,
    [__NR_sendmsg]         = sys_sendmsg,
    [__NR_recvmsg]         = sys_recvmsg,
    [__NR_shutdown]        = sys_shutdown,
    [__NR_getsockname]     = sys_getsockname,
    [__NR_socketpair]      = sys_socketpair,
    [__NR_setsockopt]      = sys_setsockopt,
    [__NR_getsockopt]      = sys_getsockopt,
    [__NR_nanosleep]       = sys_nanosleep,
    [__NR_getpid]          = sys_getpid,
    [__NR_exit]            = sys_exit,
    [__NR_fcntl]           = sys_fcntl,
    [__NR_dup]             = sys_dup,
    [__NR_dup2]            = sys_dup2,
    [__NR_getcwd]          = sys_getcwd,
    [__NR_chdir]           = sys_chdir,
    [__NR_unlink]          = sys_unlink,
    [__NR_readlink]        = sys_readlink,
    [__NR_uname]           = sys_uname,
    [__NR_mkdir]           = sys_mkdir,
    [__NR_getuid]          = sys_getuid,
    [__NR_getgid]          = sys_getgid,
    [__NR_geteuid]         = sys_geteuid,
    [__NR_getegid]         = sys_getegid,
    [__NR_arch_prctl]      = sys_arch_prctl,
    [__NR_gettid]          = sys_gettid,
    [__NR_set_tid_address] = sys_set_tid_address,
    [__NR_clock_gettime]   = sys_clock_gettime,
    [__NR_exit_group]      = sys_exit_group,
    [__NR_getdents64]      = sys_getdents64,
    [__NR_mkdirat]         = sys_mkdirat,
    [__NR_openat]          = sys_openat,
    [__NR_newfstatat]      = sys_newfstatat,
    [__NR_pread64]         = sys_pread64,
    [__NR_writev]          = sys_writev,
    [__NR_access]          = sys_access,
    [__NR_madvise]         = sys_madvise,
    [__NR_readlinkat]      = sys_readlinkat,
    [__NR_faccessat]       = sys_faccessat,
    [__NR_faccessat2]      = sys_faccessat2,
    [__NR_set_robust_list] = sys_set_robust_list,
    [__NR_futex]           = sys_futex,
    [__NR_clone]           = sys_clone,
    [__NR_prlimit64]       = sys_prlimit64,
    [__NR_getrandom]       = sys_getrandom,
    [__NR_ppoll]           = sys_ppoll,
    [__NR_statx]           = sys_statx,
};

const linux_syscall_fn_t *linux_syscall_table_ref(size_t *count_out)
{
    if (count_out != NULL) {
        *count_out = (size_t)LINUX_SYSCALL_MAX;
    }
    return linux_syscall_table;
}
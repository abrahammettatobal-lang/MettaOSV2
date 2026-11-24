#ifndef METTA_LINUX_RUNTIME_H
#define METTA_LINUX_RUNTIME_H

/*
 * Contrato Linux userspace runtime (stub SA-32).
 * Errores de syscall: valores negativos de linux_errno_t (convención dispatcher).
 */
#include <metta/types.h>

#define METTA_LINUX_RUNTIME_REV  1u

#define METTA_LINUX_MAX_PROCS    32u
#define METTA_LINUX_MAX_FDS      256u
#define METTA_LINUX_PIPE_BUF     4096u

typedef metta_u32 linux_pid_t;
typedef metta_u32 linux_fd_t;

#define LINUX_PID_INVALID  0u

typedef enum {
    LINUX_PROC_IDLE = 0,
    LINUX_PROC_RUNNABLE,
    LINUX_PROC_ZOMBIE,
    LINUX_PROC_DEAD
} linux_proc_state_t;

typedef enum {
    LINUX_FD_NONE = 0,
    LINUX_FD_TTY_STUB,
    LINUX_FD_FILE_STUB,
    LINUX_FD_PIPE_RD,
    LINUX_FD_PIPE_WR,
    LINUX_FD_X11
} linux_fd_kind_t;

typedef enum {
    LINUX_OK = 0,
    LINUX_EPERM   = 1,
    LINUX_ENOENT  = 2,
    LINUX_ESRCH     = 3,
    LINUX_EINTR   = 4,
    LINUX_EIO     = 5,
    LINUX_EBADF   = 9,
    LINUX_EAGAIN  = 11,
    LINUX_ENOMEM  = 12,
    LINUX_EACCES  = 13,
    LINUX_EFAULT  = 14,
    LINUX_EBUSY   = 16,
    LINUX_EEXIST  = 17,
    LINUX_ENODEV  = 19,
    LINUX_EISDIR  = 21,
    LINUX_EINVAL  = 22,
    LINUX_EMFILE  = 24,
    LINUX_ENOSYS  = 38,
    LINUX_EAFNOSUPPORT = 97,
    LINUX_EPIPE   = 32
} linux_errno_t;

typedef struct linux_proc linux_proc_t;

metta_status_t linux_runtime_init(void);
bool           linux_runtime_ready(void);

linux_proc_t  *linux_proc_current(void);
linux_proc_t  *linux_proc_by_pid(linux_pid_t pid);
linux_pid_t    linux_proc_pid(const linux_proc_t *proc);
linux_pid_t    linux_proc_ppid(const linux_proc_t *proc);
linux_proc_state_t linux_proc_state(const linux_proc_t *proc);

/* Procesos (stubs: sin cargar ELF ni context switch). */
metta_status_t linux_proc_spawn_stub(linux_pid_t ppid, linux_proc_t **out);
metta_status_t linux_proc_mark_zombie(linux_proc_t *proc, metta_s32 exit_code);
metta_status_t linux_proc_reap(linux_pid_t pid, metta_s32 *exit_code_out);

/*
 * Tabla de descriptores (por proceso).
 * Devuelve fd >= 0 o negación de linux_errno_t.
 */
metta_s32 linux_fd_open_stub(linux_proc_t *proc, linux_fd_kind_t kind);
metta_status_t linux_fd_close(linux_proc_t *proc, linux_fd_t fd);
linux_fd_kind_t linux_fd_kind(const linux_proc_t *proc, linux_fd_t fd);

/* pipe(2): pipefd[0] lectura, pipefd[1] escritura */
metta_s32 linux_pipe_create(linux_proc_t *proc, linux_fd_t pipefd[2]);

/* Lectura/escritura mínima sobre pipes en memoria; EBADF si no aplica. */
metta_s32 linux_pipe_read(linux_proc_t *proc, linux_fd_t fd,
                          void *buf, metta_u64 len);
metta_s32 linux_pipe_write(linux_proc_t *proc, linux_fd_t fd,
                           const void *buf, metta_u64 len);
int       linux_pipe_pending(const linux_proc_t *proc, linux_fd_t fd);

#endif /* METTA_LINUX_RUNTIME_H */

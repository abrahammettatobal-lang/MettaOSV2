/* MettaOS — Linux runtime stub (procesos, fds, pipes) — SA-32 */
#include "linux/runtime.h"

#include "linux/signal.h"

#include <stddef.h>

typedef struct {
    linux_fd_kind_t kind;
    metta_u32       pipe_id; /* par read/write comparte id */
    metta_u32       refcnt;
} linux_fd_slot_t;

typedef struct {
    metta_u32  id;
    metta_u8   buf[METTA_LINUX_PIPE_BUF];
    metta_u64  head;
    metta_u64  tail;
    metta_u64  count;
    int        read_open;
    int        write_open;
} linux_pipe_t;

#define METTA_LINUX_MAX_PIPES 16u

struct linux_proc {
    int                  in_use;
    linux_pid_t          pid;
    linux_pid_t          ppid;
    linux_proc_state_t   state;
    metta_s32            exit_code;
    linux_fd_slot_t      fds[METTA_LINUX_MAX_FDS];
    linux_signal_state_t signals;
};

static linux_proc_t     g_procs[METTA_LINUX_MAX_PROCS];
static linux_pipe_t     g_pipes[METTA_LINUX_MAX_PIPES];
static linux_pid_t      g_next_pid = 1u;
static linux_proc_t    *g_current;
static int              g_ready;

static metta_s32 linux_errno_neg(linux_errno_t err)
{
    return -(metta_s32)err;
}

static linux_pipe_t *pipe_by_id(metta_u32 id)
{
    if (id == 0 || id > METTA_LINUX_MAX_PIPES) {
        return NULL;
    }
    linux_pipe_t *p = &g_pipes[id - 1u];
    return p->id != 0 ? p : NULL;
}

static metta_status_t pipe_alloc(linux_pipe_t **out, metta_u32 *id_out)
{
    for (metta_u32 i = 0; i < METTA_LINUX_MAX_PIPES; i++) {
        if (g_pipes[i].id == 0) {
            linux_pipe_t *p = &g_pipes[i];
            p->id = i + 1u;
            p->head = 0;
            p->tail = 0;
            p->count = 0;
            p->read_open = 1;
            p->write_open = 1;
            *out = p;
            *id_out = p->id;
            return METTA_OK;
        }
    }
    return METTA_ERR_NO_MEMORY;
}

static void pipe_release(metta_u32 pipe_id)
{
    linux_pipe_t *p = pipe_by_id(pipe_id);
    if (p == NULL) {
        return;
    }
    p->id = 0;
    p->head = p->tail = p->count = 0;
    p->read_open = p->write_open = 0;
}

static linux_proc_t *proc_slot_alloc(linux_pid_t ppid)
{
    for (metta_u32 i = 0; i < METTA_LINUX_MAX_PROCS; i++) {
        if (!g_procs[i].in_use) {
            linux_proc_t *p = &g_procs[i];
            p->in_use = 1;
            p->pid = g_next_pid++;
            if (p->pid == LINUX_PID_INVALID) {
                p->pid = g_next_pid++;
            }
            p->ppid = ppid;
            p->state = LINUX_PROC_RUNNABLE;
            p->exit_code = 0;
            for (metta_u32 fd = 0; fd < METTA_LINUX_MAX_FDS; fd++) {
                p->fds[fd].kind = LINUX_FD_NONE;
                p->fds[fd].pipe_id = 0;
                p->fds[fd].refcnt = 0;
            }
            if (linux_signal_init_proc(p) != METTA_OK) {
                p->in_use = 0;
                return NULL;
            }
            return p;
        }
    }
    return NULL;
}

static void proc_slot_free(linux_proc_t *proc)
{
    if (proc == NULL || !proc->in_use) {
        return;
    }
    for (metta_u32 fd = 0; fd < METTA_LINUX_MAX_FDS; fd++) {
        if (proc->fds[fd].kind == LINUX_FD_PIPE_RD ||
            proc->fds[fd].kind == LINUX_FD_PIPE_WR) {
            linux_pipe_t *pipe = pipe_by_id(proc->fds[fd].pipe_id);
            if (pipe != NULL) {
                if (proc->fds[fd].kind == LINUX_FD_PIPE_RD) {
                    pipe->read_open = 0;
                } else {
                    pipe->write_open = 0;
                }
                if (!pipe->read_open && !pipe->write_open) {
                    pipe_release(proc->fds[fd].pipe_id);
                }
            }
        }
    }
    linux_signal_teardown_proc(proc);
    proc->in_use = 0;
    proc->state = LINUX_PROC_DEAD;
    if (g_current == proc) {
        g_current = NULL;
    }
}

linux_signal_state_t *linux_proc_signal_state(linux_proc_t *proc)
{
    if (proc == NULL || !proc->in_use) {
        return NULL;
    }
    return &proc->signals;
}

metta_status_t linux_runtime_init(void)
{
    if (g_ready) {
        return METTA_OK;
    }

    for (metta_u32 i = 0; i < METTA_LINUX_MAX_PROCS; i++) {
        g_procs[i].in_use = 0;
    }
    for (metta_u32 i = 0; i < METTA_LINUX_MAX_PIPES; i++) {
        g_pipes[i].id = 0;
    }

    linux_proc_t *init = proc_slot_alloc(LINUX_PID_INVALID);
    if (init == NULL) {
        return METTA_ERR_NO_MEMORY;
    }
    init->ppid = LINUX_PID_INVALID;

    /* stdin/stdout/stderr → TTY stub */
    for (int stdfd = 0; stdfd < 3; stdfd++) {
        init->fds[(metta_u32)stdfd].kind = LINUX_FD_TTY_STUB;
        init->fds[(metta_u32)stdfd].refcnt = 1;
    }

    g_current = init;
    g_ready = 1;
    return METTA_OK;
}

bool linux_runtime_ready(void)
{
    return g_ready != 0;
}

linux_proc_t *linux_proc_current(void)
{
    return g_current;
}

linux_proc_t *linux_proc_by_pid(linux_pid_t pid)
{
    if (pid == LINUX_PID_INVALID) {
        return NULL;
    }
    for (metta_u32 i = 0; i < METTA_LINUX_MAX_PROCS; i++) {
        if (g_procs[i].in_use && g_procs[i].pid == pid) {
            return &g_procs[i];
        }
    }
    return NULL;
}

linux_pid_t linux_proc_pid(const linux_proc_t *proc)
{
    return proc != NULL ? proc->pid : LINUX_PID_INVALID;
}

linux_pid_t linux_proc_ppid(const linux_proc_t *proc)
{
    return proc != NULL ? proc->ppid : LINUX_PID_INVALID;
}

linux_proc_state_t linux_proc_state(const linux_proc_t *proc)
{
    return proc != NULL ? proc->state : LINUX_PROC_DEAD;
}

metta_status_t linux_proc_spawn_stub(linux_pid_t ppid, linux_proc_t **out)
{
    if (!g_ready || out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    linux_proc_t *child = proc_slot_alloc(ppid);
    if (child == NULL) {
        return METTA_ERR_NO_MEMORY;
    }
    *out = child;
    return METTA_OK;
}

metta_status_t linux_proc_mark_zombie(linux_proc_t *proc, metta_s32 exit_code)
{
    if (proc == NULL || !proc->in_use) {
        return METTA_ERR_INVALID_ARG;
    }
    proc->state = LINUX_PROC_ZOMBIE;
    proc->exit_code = exit_code;
    return METTA_OK;
}

metta_status_t linux_proc_reap(linux_pid_t pid, metta_s32 *exit_code_out)
{
    linux_proc_t *proc = linux_proc_by_pid(pid);
    if (proc == NULL) {
        return METTA_ERR_NOT_FOUND;
    }
    if (proc->state != LINUX_PROC_ZOMBIE) {
        return METTA_ERR_BUSY;
    }
    if (exit_code_out != NULL) {
        *exit_code_out = proc->exit_code;
    }
    proc_slot_free(proc);
    return METTA_OK;
}

metta_s32 linux_fd_open_stub(linux_proc_t *proc, linux_fd_kind_t kind)
{
    if (proc == NULL || !proc->in_use) {
        return linux_errno_neg(LINUX_EFAULT);
    }
    if (kind == LINUX_FD_NONE) {
        return linux_errno_neg(LINUX_EINVAL);
    }
    for (metta_u32 fd = 0; fd < METTA_LINUX_MAX_FDS; fd++) {
        if (proc->fds[fd].kind == LINUX_FD_NONE) {
            proc->fds[fd].kind = kind;
            proc->fds[fd].pipe_id = 0;
            proc->fds[fd].refcnt = 1;
            return (metta_s32)fd;
        }
    }
    return linux_errno_neg(LINUX_EMFILE);
}

metta_status_t linux_fd_close(linux_proc_t *proc, linux_fd_t fd)
{
    if (proc == NULL || !proc->in_use) {
        return METTA_ERR_INVALID_ARG;
    }
    if (fd >= METTA_LINUX_MAX_FDS) {
        return METTA_ERR_RANGE;
    }
    linux_fd_slot_t *slot = &proc->fds[fd];
    if (slot->kind == LINUX_FD_NONE) {
        return METTA_ERR_NOT_FOUND;
    }

    if (slot->kind == LINUX_FD_PIPE_RD || slot->kind == LINUX_FD_PIPE_WR) {
        linux_pipe_t *pipe = pipe_by_id(slot->pipe_id);
        if (pipe != NULL) {
            if (slot->kind == LINUX_FD_PIPE_RD) {
                pipe->read_open = 0;
            } else {
                pipe->write_open = 0;
            }
            if (!pipe->read_open && !pipe->write_open) {
                pipe_release(slot->pipe_id);
            } else if (!pipe->write_open && pipe->read_open) {
                /* EPIPE al escritor ya cerrado: SIGPIPE pendiente (stub). */
                linux_kill(proc->pid, LINUX_SIGPIPE);
            }
        }
    }

    slot->kind = LINUX_FD_NONE;
    slot->pipe_id = 0;
    slot->refcnt = 0;
    return METTA_OK;
}

linux_fd_kind_t linux_fd_kind(const linux_proc_t *proc, linux_fd_t fd)
{
    if (proc == NULL || !proc->in_use || fd >= METTA_LINUX_MAX_FDS) {
        return LINUX_FD_NONE;
    }
    return proc->fds[fd].kind;
}

metta_s32 linux_pipe_create(linux_proc_t *proc, linux_fd_t pipefd[2])
{
    if (proc == NULL || !proc->in_use || pipefd == NULL) {
        return linux_errno_neg(LINUX_EFAULT);
    }

    linux_pipe_t *pipe = NULL;
    metta_u32     pipe_id = 0;
    if (pipe_alloc(&pipe, &pipe_id) != METTA_OK) {
        return linux_errno_neg(LINUX_ENOMEM);
    }

    metta_s32 rd = linux_fd_open_stub(proc, LINUX_FD_PIPE_RD);
    if (rd < 0) {
        pipe_release(pipe_id);
        return rd;
    }
    metta_s32 wr = linux_fd_open_stub(proc, LINUX_FD_PIPE_WR);
    if (wr < 0) {
        (void)linux_fd_close(proc, (linux_fd_t)rd);
        pipe_release(pipe_id);
        return wr;
    }

    proc->fds[(metta_u32)rd].pipe_id = pipe_id;
    proc->fds[(metta_u32)wr].pipe_id = pipe_id;

    pipefd[0] = (linux_fd_t)rd;
    pipefd[1] = (linux_fd_t)wr;
    return LINUX_OK;
}

metta_s32 linux_pipe_read(linux_proc_t *proc, linux_fd_t fd,
                          void *buf, metta_u64 len)
{
    if (proc == NULL || !proc->in_use || buf == NULL) {
        return linux_errno_neg(LINUX_EFAULT);
    }
    if (fd >= METTA_LINUX_MAX_FDS ||
        proc->fds[fd].kind != LINUX_FD_PIPE_RD) {
        return linux_errno_neg(LINUX_EBADF);
    }

    linux_pipe_t *pipe = pipe_by_id(proc->fds[fd].pipe_id);
    if (pipe == NULL) {
        return linux_errno_neg(LINUX_EINVAL);
    }
    if (len == 0) {
        return 0;
    }
    if (pipe->count == 0) {
        if (!pipe->write_open) {
            return 0;
        }
        return linux_errno_neg(LINUX_EAGAIN);
    }

    metta_u64 n = len;
    if (n > pipe->count) {
        n = pipe->count;
    }
    const metta_u8 *ring = pipe->buf;
    metta_u8 *out = (metta_u8 *)buf;
    for (metta_u64 i = 0; i < n; i++) {
        out[i] = ring[pipe->head];
        pipe->head = (pipe->head + 1u) % METTA_LINUX_PIPE_BUF;
    }
    pipe->count -= n;
    return (metta_s32)n;
}

metta_s32 linux_pipe_write(linux_proc_t *proc, linux_fd_t fd,
                           const void *buf, metta_u64 len)
{
    if (proc == NULL || !proc->in_use || buf == NULL) {
        return linux_errno_neg(LINUX_EFAULT);
    }
    if (fd >= METTA_LINUX_MAX_FDS ||
        proc->fds[fd].kind != LINUX_FD_PIPE_WR) {
        return linux_errno_neg(LINUX_EBADF);
    }

    linux_pipe_t *pipe = pipe_by_id(proc->fds[fd].pipe_id);
    if (pipe == NULL) {
        return linux_errno_neg(LINUX_EINVAL);
    }
    if (!pipe->read_open) {
        linux_kill(proc->pid, LINUX_SIGPIPE);
        return linux_errno_neg(LINUX_EPIPE);
    }
    if (len == 0) {
        return 0;
    }

    metta_u64 space = METTA_LINUX_PIPE_BUF - pipe->count;
    if (space == 0) {
        return linux_errno_neg(LINUX_EAGAIN);
    }
    metta_u64 n = len;
    if (n > space) {
        n = space;
    }

    const metta_u8 *in = (const metta_u8 *)buf;
    for (metta_u64 i = 0; i < n; i++) {
        pipe->buf[pipe->tail] = in[i];
        pipe->tail = (pipe->tail + 1u) % METTA_LINUX_PIPE_BUF;
    }
    pipe->count += n;
    return (metta_s32)n;
}

int linux_pipe_pending(const linux_proc_t *proc, linux_fd_t fd)
{
    const linux_pipe_t *pipe;

    if (proc == NULL || !proc->in_use || fd >= METTA_LINUX_MAX_FDS) {
        return 0;
    }
    if (proc->fds[fd].kind != LINUX_FD_PIPE_RD) {
        return 0;
    }
    pipe = pipe_by_id(proc->fds[fd].pipe_id);
    return pipe != NULL && pipe->count > 0u;
}
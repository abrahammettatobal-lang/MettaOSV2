#ifndef METTA_LINUX_SIGNAL_H
#define METTA_LINUX_SIGNAL_H

#include <metta/types.h>

#include "linux/runtime.h"

#define METTA_LINUX_SIGNAL_REV  1u

#define LINUX_NSIG              64

#define LINUX_SIG_ERR  ((void (*)(int))-1)
#define LINUX_SIG_DFL  ((void (*)(int))0)
#define LINUX_SIG_IGN  ((void (*)(int))1)

#define LINUX_SIGHUP   1
#define LINUX_SIGINT   2
#define LINUX_SIGQUIT  3
#define LINUX_SIGILL   4
#define LINUX_SIGTRAP  5
#define LINUX_SIGABRT  6
#define LINUX_SIGBUS   7
#define LINUX_SIGFPE   8
#define LINUX_SIGKILL  9
#define LINUX_SIGUSR1  10
#define LINUX_SIGSEGV  11
#define LINUX_SIGUSR2  12
#define LINUX_SIGPIPE  13
#define LINUX_SIGALRM  14
#define LINUX_SIGTERM  15
#define LINUX_SIGSTOP  19

#define LINUX_SIG_BLOCK   0
#define LINUX_SIG_UNBLOCK 1
#define LINUX_SIG_SETMASK 2

typedef metta_u64 linux_sigset_t;

typedef void (*linux_sighandler_t)(int sig);

typedef struct {
    linux_sighandler_t handler;
    linux_sigset_t     mask;
    metta_u32          flags;
} linux_sigaction_t;

typedef struct {
    volatile metta_u64 pending;
    volatile metta_u64 blocked;
    linux_sigaction_t  actions[LINUX_NSIG];
} linux_signal_state_t;

/* Acceso interno desde runtime.c (misma subunidad SA-32). */
linux_signal_state_t *linux_proc_signal_state(linux_proc_t *proc);

metta_status_t linux_signal_init_proc(linux_proc_t *proc);
void           linux_signal_teardown_proc(linux_proc_t *proc);

metta_s32 linux_kill(linux_pid_t pid, int sig);
metta_s32 linux_tgkill(linux_pid_t tgid, linux_pid_t tid, int sig);

metta_s32 linux_sigaction(linux_proc_t *proc, int sig,
                          const linux_sigaction_t *act,
                          linux_sigaction_t *old);

metta_s32 linux_sigprocmask(linux_proc_t *proc, int how,
                            const linux_sigset_t *set,
                            linux_sigset_t *oldset);

/* Entrega stub: marca pending; sin saltar a userspace. */
metta_status_t linux_signal_deliver_pending(linux_proc_t *proc);

metta_u64 linux_signal_pending_mask(const linux_proc_t *proc);

#endif /* METTA_LINUX_SIGNAL_H */
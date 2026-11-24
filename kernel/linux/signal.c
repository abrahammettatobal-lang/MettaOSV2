/* MettaOS — Linux signals stub — SA-32 */
#include "linux/signal.h"

#include <stddef.h>

static linux_signal_state_t *signals_of(linux_proc_t *proc)
{
    return linux_proc_signal_state(proc);
}

static int signal_valid(int sig)
{
    return sig > 0 && sig < LINUX_NSIG;
}

static int sig_member(linux_sigset_t set, int sig)
{
    if (!signal_valid(sig)) {
        return 0;
    }
    return (set & ((linux_sigset_t)1 << (sig - 1))) != 0;
}

static void pending_or(linux_signal_state_t *st, int sig)
{
    if (st == NULL || !signal_valid(sig)) {
        return;
    }
    st->pending |= (linux_sigset_t)1 << (sig - 1);
}

metta_status_t linux_signal_init_proc(linux_proc_t *proc)
{
    linux_signal_state_t *st = signals_of(proc);
    if (st == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    st->pending = 0;
    st->blocked = 0;
    for (int i = 0; i < LINUX_NSIG; i++) {
        st->actions[i].handler = LINUX_SIG_DFL;
        st->actions[i].mask = 0;
        st->actions[i].flags = 0;
    }
    return METTA_OK;
}

void linux_signal_teardown_proc(linux_proc_t *proc)
{
    linux_signal_state_t *st = signals_of(proc);
    if (st == NULL) {
        return;
    }
    st->pending = 0;
    st->blocked = 0;
}

metta_s32 linux_kill(linux_pid_t pid, int sig)
{
    if (!linux_runtime_ready()) {
        return -(metta_s32)LINUX_EINVAL;
    }
    if (pid == 0) {
        pid = linux_proc_pid(linux_proc_current());
    }
    if (!signal_valid(sig)) {
        return -(metta_s32)LINUX_EINVAL;
    }

    linux_proc_t *proc = linux_proc_by_pid(pid);
    if (proc == NULL) {
        return -(metta_s32)LINUX_ESRCH;
    }

    linux_signal_state_t *st = signals_of(proc);
    if (st == NULL) {
        return -(metta_s32)LINUX_EINVAL;
    }

    if (sig == LINUX_SIGKILL || sig == LINUX_SIGSTOP) {
        pending_or(st, sig);
        return LINUX_OK;
    }

    if (sig_member(st->blocked, sig)) {
        pending_or(st, sig);
        return LINUX_OK;
    }

    pending_or(st, sig);
    return LINUX_OK;
}

metta_s32 linux_tgkill(linux_pid_t tgid, linux_pid_t tid, int sig)
{
    (void)tid;
    return linux_kill(tgid, sig);
}

metta_s32 linux_sigaction(linux_proc_t *proc, int sig,
                          const linux_sigaction_t *act,
                          linux_sigaction_t *old)
{
    if (proc == NULL || !signal_valid(sig)) {
        return -(metta_s32)LINUX_EINVAL;
    }
    linux_signal_state_t *st = signals_of(proc);
    if (st == NULL) {
        return -(metta_s32)LINUX_EINVAL;
    }
    if (sig == LINUX_SIGKILL || sig == LINUX_SIGSTOP) {
        return -(metta_s32)LINUX_EINVAL;
    }

    linux_sigaction_t *slot = &st->actions[sig];
    if (old != NULL) {
        *old = *slot;
    }
    if (act != NULL) {
        *slot = *act;
    }
    return LINUX_OK;
}

metta_s32 linux_sigprocmask(linux_proc_t *proc, int how,
                            const linux_sigset_t *set,
                            linux_sigset_t *oldset)
{
    if (proc == NULL) {
        return -(metta_s32)LINUX_EINVAL;
    }
    linux_signal_state_t *st = signals_of(proc);
    if (st == NULL) {
        return -(metta_s32)LINUX_EINVAL;
    }

    if (oldset != NULL) {
        *oldset = st->blocked;
    }
    if (set == NULL) {
        return LINUX_OK;
    }

    switch (how) {
    case LINUX_SIG_BLOCK:
        st->blocked |= *set;
        break;
    case LINUX_SIG_UNBLOCK:
        st->blocked &= ~(*set);
        break;
    case LINUX_SIG_SETMASK:
        st->blocked = *set;
        break;
    default:
        return -(metta_s32)LINUX_EINVAL;
    }
    return LINUX_OK;
}

metta_status_t linux_signal_deliver_pending(linux_proc_t *proc)
{
    linux_signal_state_t *st = signals_of(proc);
    if (st == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    for (int sig = 1; sig < LINUX_NSIG; sig++) {
        if (!sig_member(st->pending, sig)) {
            continue;
        }
        if (sig_member(st->blocked, sig)) {
            continue;
        }
        /* Stub: consume pending; no trampoline a userspace. */
        st->pending &= ~((linux_sigset_t)1 << (sig - 1));
        if (sig == LINUX_SIGKILL) {
            (void)linux_proc_mark_zombie(proc, 128 + sig);
        }
    }
    return METTA_OK;
}

metta_u64 linux_signal_pending_mask(const linux_proc_t *proc)
{
    const linux_signal_state_t *st =
        linux_proc_signal_state((linux_proc_t *)proc);
    if (st == NULL) {
        return 0;
    }
    return st->pending;
}
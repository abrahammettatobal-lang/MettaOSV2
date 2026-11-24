/*
 * Dispatcher de syscalls Linux x86_64 → servicios MettaOS (vía linux_rt_*).
 * Convención de retorno: valor de éxito en rax, o -(errno Linux) en error.
 */
#include "syscall_linux.h"

#include <linux/runtime.h>

#include <metta/types.h>

static uint64_t g_linux_syscall_enosys;

void linux_syscall_init(void)
{
    g_linux_syscall_enosys = 0;
    (void)linux_runtime_init();
}

uint64_t linux_syscall_enosys_count(void)
{
    return g_linux_syscall_enosys;
}

int linux_errno_from_metta(int metta_status)
{
    switch ((metta_status_t)metta_status) {
    case METTA_OK:
        return 0;
    case METTA_ERR_INVALID_ARG:
        return LINUX_EINVAL;
    case METTA_ERR_NO_MEMORY:
        return LINUX_ENOMEM;
    case METTA_ERR_NOT_FOUND:
        return LINUX_ENOENT;
    case METTA_ERR_BUSY:
        return LINUX_EBUSY;
    case METTA_ERR_PERM:
        return LINUX_EACCES;
    case METTA_ERR_RANGE:
        return LINUX_EFAULT;
    case METTA_ERR_UNINITIALIZED:
    case METTA_ERR_INTERNAL:
    case METTA_ERR_CORRUPT:
    default:
        return LINUX_EINVAL;
    }
}

int64_t linux_syscall_dispatch_raw(uint64_t nr, uint64_t rdi, uint64_t rsi, uint64_t rdx,
                                     uint64_t r10, uint64_t r8, uint64_t r9)
{
    linux_syscall_frame_t frame = {
        .nr  = nr,
        .rdi = rdi,
        .rsi = rsi,
        .rdx = rdx,
        .r10 = r10,
        .r8  = r8,
        .r9  = r9,
    };
    return linux_syscall_dispatch(&frame);
}

int64_t linux_syscall_dispatch(const linux_syscall_frame_t *frame)
{
    size_t            table_len = 0;
    const linux_syscall_fn_t *table;
    linux_syscall_fn_t handler;
    int64_t           ret;

    if (frame == NULL) {
        return -(int64_t)LINUX_EINVAL;
    }

    table = linux_syscall_table_ref(&table_len);

    if (frame->nr >= table_len) {
        g_linux_syscall_enosys++;
        return -(int64_t)LINUX_ENOSYS;
    }

    handler = table[frame->nr];
    if (handler == NULL) {
        g_linux_syscall_enosys++;
        return -(int64_t)LINUX_ENOSYS;
    }

    ret = handler(frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);

    if (ret == -(int64_t)LINUX_ENOSYS) {
        g_linux_syscall_enosys++;
    }

    return ret;
}
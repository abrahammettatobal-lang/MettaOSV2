#include "syscall/metta_abi.h"

#include <stddef.h>
#include <stdint.h>

typedef metta_syscall_ret_t (*sys_handler_t)(
    uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static sys_handler_t syscall_table[METTA_SYSCALL_TABLE_SIZE];

static uint64_t stub_current_pid = 0u;
static int      stub_user_context;

static int user_range_ok(uintptr_t start, size_t len)
{
    if (len == 0u) {
        return (start >= METTA_USER_ADDR_MIN && start <= METTA_USER_ADDR_MAX) ? 1 : 0;
    }
    if (start < METTA_USER_ADDR_MIN) {
        return 0;
    }
    if (start > METTA_USER_ADDR_MAX) {
        return 0;
    }
    if ((start >> 47) != 0u) {
        return 0;
    }
    uintptr_t end = start + len - 1u;
    if (end < start) {
        return 0;
    }
    if (end > METTA_USER_ADDR_MAX) {
        return 0;
    }
    return 1;
}

int metta_user_ptr_validate(const void *ptr, size_t len, uint32_t access)
{
    (void)access;

    if (ptr == NULL) {
        return METTA_ABI_EINVAL;
    }

    uintptr_t start = (uintptr_t)ptr;
    if (!user_range_ok(start, len)) {
        return METTA_ABI_EFAULT;
    }

    /*
     * Futuro (SA-08/09): vmm_probe_user_range(as, start, len, access).
     * Por ahora solo rechazamos punteros kernel obvios y rangos inválidos.
     */
    return METTA_ABI_OK;
}

int metta_user_cstr_validate(const char *str, size_t max_len)
{
    uintptr_t p;

    if (str == NULL) {
        return METTA_ABI_EINVAL;
    }
    if (max_len == 0u) {
        return METTA_ABI_EINVAL;
    }

    p = (uintptr_t)str;
    if (p < METTA_USER_ADDR_MIN || p > METTA_USER_ADDR_MAX) {
        return METTA_ABI_EFAULT;
    }

    for (size_t i = 0u; i < max_len; i++) {
        if (!user_range_ok(p + i, 1u)) {
            return METTA_ABI_EFAULT;
        }
        if (str[i] == '\0') {
            return METTA_ABI_OK;
        }
    }

    return METTA_ABI_EINVAL;
}

uint64_t metta_syscall_current_pid(void)
{
    return stub_current_pid;
}

int metta_syscall_is_user_context(void)
{
    return stub_user_context;
}

void metta_syscall_set_context(uint64_t pid, int is_user)
{
    stub_current_pid = pid;
    stub_user_context = (is_user != 0) ? 1 : 0;
}

/* --- handlers mínimos --- */

static metta_syscall_ret_t sys_nop(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return METTA_ABI_OK;
}

static metta_syscall_ret_t sys_exit(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    /*
     * SA-09: proc_mark_zombie(metta_syscall_current_pid(), (int)a0);
     * Por ahora devolvemos OK sin terminar el hilo.
     */
    (void)a0;
    return METTA_ABI_OK;
}

static metta_syscall_ret_t sys_yield(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    /* SA-09: sched_yield(); */
    return METTA_ABI_OK;
}

static metta_syscall_ret_t sys_getpid(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return (metta_syscall_ret_t)metta_syscall_current_pid();
}

static metta_syscall_ret_t sys_write(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    const void *buf = (const void *)(uintptr_t)a1;
    size_t len = (size_t)a2;
    int v;

    (void)a3;
    (void)a4;
    (void)a5;

    if (a0 != 1u && a0 != 2u) {
        return METTA_ABI_EINVAL;
    }

    v = metta_user_ptr_validate(buf, len, METTA_PTR_ACCESS_READ);
    if (v != METTA_ABI_OK) {
        return v;
    }

    /* Sin VFS/TTY aún: simula escritura completa */
    (void)buf;
    return (metta_syscall_ret_t)len;
}

static metta_syscall_ret_t sys_read(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    void *buf = (void *)(uintptr_t)a1;
    size_t len = (size_t)a2;
    int v;

    (void)a3;
    (void)a4;
    (void)a5;

    if (a0 != 0u) {
        return METTA_ABI_EINVAL;
    }
    if (len > 0u) {
        v = metta_user_ptr_validate(buf, len, METTA_PTR_ACCESS_WRITE);
        if (v != METTA_ABI_OK) {
            return v;
        }
    }
    return 0;
}

static metta_syscall_ret_t sys_enosys(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return METTA_ABI_ENOSYS;
}

static metta_syscall_ret_t sys_mde_hello(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    const char *name = (const char *)(uintptr_t)a0;
    int v;

    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;

    if (!metta_syscall_is_user_context()) {
        return METTA_ABI_EPERM;
    }

    v = metta_user_cstr_validate(name, 256u);
    if (v != METTA_ABI_OK) {
        return v;
    }

    /* SA-15: registro de cliente MDE en compositor */
    return METTA_ABI_OK;
}

static metta_syscall_ret_t sys_mde_win_stub(
    uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;

    if (!metta_syscall_is_user_context()) {
        return METTA_ABI_EPERM;
    }
    return METTA_ABI_ENOTSUP;
}

static void register_syscall(uint32_t nr, sys_handler_t fn)
{
    if (nr >= METTA_SYSCALL_TABLE_SIZE) {
        return;
    }
    syscall_table[nr] = fn;
}

void metta_syscall_init(void)
{
    for (uint32_t i = 0u; i < METTA_SYSCALL_TABLE_SIZE; i++) {
        syscall_table[i] = sys_enosys;
    }

    register_syscall(METTA_SYS_NOP, sys_nop);
    register_syscall(METTA_SYS_EXIT, sys_exit);
    register_syscall(METTA_SYS_YIELD, sys_yield);
    register_syscall(METTA_SYS_GETPID, sys_getpid);
    register_syscall(METTA_SYS_WRITE, sys_write);
    register_syscall(METTA_SYS_READ, sys_read);

    register_syscall(METTA_SYS_MDE_HELLO, sys_mde_hello);
    register_syscall(METTA_SYS_MDE_WIN_OPEN, sys_mde_win_stub);
    register_syscall(METTA_SYS_MDE_WIN_CLOSE, sys_mde_win_stub);
    register_syscall(METTA_SYS_MDE_WIN_RESIZE, sys_mde_win_stub);
    register_syscall(METTA_SYS_MDE_FB_BLIT, sys_mde_win_stub);

    stub_current_pid = 0u;
    stub_user_context = 0;
}

metta_syscall_ret_t metta_syscall_dispatch(uint32_t nr,
                                           uint64_t a0, uint64_t a1, uint64_t a2,
                                           uint64_t a3, uint64_t a4, uint64_t a5)
{
    sys_handler_t fn;

    if (nr >= METTA_SYSCALL_TABLE_SIZE) {
        return METTA_ABI_ENOSYS;
    }

    fn = syscall_table[nr];
    if (fn == NULL) {
        return METTA_ABI_ENOSYS;
    }

    return fn(a0, a1, a2, a3, a4, a5);
}
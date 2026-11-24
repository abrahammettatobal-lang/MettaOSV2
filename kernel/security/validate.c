#include "metta/validate.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Bit 47 en direcciones canónicas de usuario: todo 0 en la mitad baja.
 * Cualquier bit en 0xFFFF800000000000 en la mitad baja indica kernel/HHDM.
 */
#define METTA_VIRT_UPPER_REJECT_MASK  0xFFFF800000000000ULL

#define METTA_USER_STACK_GUARD_BYTES  METTA_PAGE_SIZE

static metta_sec_context_t   g_ctx;
static metta_sec_vmm_probe_fn g_probe;
static int                   g_ready;

static int metta_sec_started(void)
{
    return g_ready != 0;
}

void metta_sec_init(void)
{
    g_ctx.user_stack_top = 0;
    g_probe              = NULL;
    g_ready              = 1;
}

void metta_sec_set_context(const metta_sec_context_t *ctx)
{
    if (ctx == NULL) {
        g_ctx.user_stack_top = 0;
        return;
    }
    g_ctx = *ctx;
}

void metta_sec_set_vmm_probe(metta_sec_vmm_probe_fn fn)
{
    g_probe = fn;
}

bool metta_sec_is_user_canonical(metta_virt_addr_t addr)
{
    if ((addr & METTA_VIRT_UPPER_REJECT_MASK) != 0) {
        return false;
    }
    if (addr > METTA_USER_VIRT_MAX) {
        return false;
    }
    return true;
}

bool metta_sec_is_kernel_virt(metta_virt_addr_t addr)
{
    return addr >= 0xFFFF800000000000ULL;
}

static metta_status_t metta_sec_check_stack_policy(metta_virt_addr_t start,
                                                    metta_virt_addr_t end_incl)
{
    metta_virt_addr_t top = g_ctx.user_stack_top;

    if (top == 0) {
        return METTA_OK;
    }

    if (top <= METTA_USER_VIRT_MIN) {
        return METTA_ERR_INTERNAL;
    }

    if (start >= top) {
        return METTA_ERR_RANGE;
    }

    if (end_incl >= top) {
        return METTA_ERR_RANGE;
    }

    if (top < METTA_USER_STACK_GUARD_BYTES + METTA_USER_VIRT_MIN) {
        return METTA_ERR_INTERNAL;
    }

    {
        metta_virt_addr_t guard_lo = top - METTA_USER_STACK_GUARD_BYTES;

        if (end_incl >= guard_lo && start < top) {
            return METTA_ERR_RANGE;
        }
    }

    return METTA_OK;
}

static metta_status_t metta_sec_range_core(metta_virt_addr_t start,
                                            metta_u64         len,
                                            metta_user_range_t *out)
{
    metta_virt_addr_t end_incl;
    metta_status_t    st;

    if (!metta_sec_started()) {
        return METTA_ERR_UNINITIALIZED;
    }

    if (len == 0) {
        if (out != NULL) {
            out->virt_start = start;
            out->virt_end   = start;
            out->length     = 0;
        }
        return METTA_OK;
    }

    if (start < METTA_USER_VIRT_MIN) {
        return METTA_ERR_RANGE;
    }

    if (!metta_sec_is_user_canonical(start)) {
        if (metta_sec_is_kernel_virt(start)) {
            return METTA_ERR_PERM;
        }
        return METTA_ERR_RANGE;
    }

    if (len > METTA_USER_VIRT_MAX - start + 1ULL) {
        return METTA_ERR_RANGE;
    }

    end_incl = start + len - 1ULL;
    if (end_incl > METTA_USER_VIRT_MAX || end_incl < start) {
        return METTA_ERR_RANGE;
    }

    st = metta_sec_check_stack_policy(start, end_incl);
    if (st != METTA_OK) {
        return st;
    }

    if (out != NULL) {
        out->virt_start = start;
        out->virt_end   = end_incl + 1ULL;
        out->length     = len;
    }

    return METTA_OK;
}

metta_status_t metta_sec_validate_user_ptr(const void *ptr)
{
    if (ptr == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    return metta_sec_range_core((metta_virt_addr_t)(uintptr_t)ptr, 1ULL, NULL);
}

metta_status_t metta_sec_validate_user_range(const void *ptr, metta_u64 len)
{
    if (ptr == NULL && len != 0) {
        return METTA_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return METTA_OK;
    }
    if (ptr == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    return metta_sec_range_core((metta_virt_addr_t)(uintptr_t)ptr, len, NULL);
}

metta_status_t metta_sec_validate_user_range_access(const void *ptr,
                                                      metta_u64     len,
                                                      metta_user_access_t access)
{
    metta_status_t st;

    st = metta_sec_validate_user_range(ptr, len);
    if (st != METTA_OK) {
        return st;
    }

    if (len == 0 || g_probe == NULL || access == 0) {
        return METTA_OK;
    }

    return g_probe((metta_virt_addr_t)(uintptr_t)ptr, len, access);
}

metta_status_t metta_sec_validate_user_cstr(const char *ptr, metta_u64 max_scan)
{
    /*
     * Solo comprueba que exista un buffer usuario acotado para escanear.
     * No lee la memoria usuario: el NUL se valida tras copy_from_user (SA-10).
     */
    if (ptr == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    if (max_scan == 0) {
        return METTA_ERR_INVALID_ARG;
    }

    return metta_sec_validate_user_range(ptr, max_scan);
}

metta_status_t metta_sec_fill_user_range(const void *ptr, metta_u64 len,
                                         metta_user_range_t *out)
{
    if (out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (len == 0) {
        out->virt_start = ptr != NULL ? (metta_virt_addr_t)(uintptr_t)ptr : 0;
        out->virt_end   = out->virt_start;
        out->length     = 0;
        return METTA_OK;
    }
    if (ptr == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    return metta_sec_range_core((metta_virt_addr_t)(uintptr_t)ptr, len, out);
}
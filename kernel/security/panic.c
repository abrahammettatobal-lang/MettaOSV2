#include "metta/panic.h"

static metta_panic_action_t g_action = METTA_PANIC_ACT_LOG_HALT;
static metta_panic_diag_fn    g_diag;

static void metta_panic_halt(void) __attribute__((noreturn));
static void metta_panic_halt(void)
{
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void metta_panic_set_action(metta_panic_action_t action)
{
    g_action = action;
}

metta_panic_action_t metta_panic_get_action(void)
{
    return g_action;
}

void metta_panic_register_diag(metta_panic_diag_fn fn)
{
    g_diag = fn;
}

void metta_panic(metta_panic_class_t cls,
                 const char           *where,
                 const char           *detail)
{
    if (g_action == METTA_PANIC_ACT_LOG_HALT && g_diag != NULL) {
        g_diag(cls, METTA_ERR_CORRUPT, where, detail != NULL ? detail : "");
    }
    metta_panic_halt();
}

void metta_panic_status(metta_panic_class_t cls,
                          metta_status_t       st,
                          const char          *where,
                          const char          *detail)
{
    if (g_action == METTA_PANIC_ACT_LOG_HALT && g_diag != NULL) {
        g_diag(cls, st, where, detail != NULL ? detail : "");
    }
    metta_panic_halt();
}

metta_status_t metta_panic_on_corrupt(metta_status_t st, const char *where)
{
    if (st == METTA_ERR_CORRUPT) {
        metta_panic(METTA_PANIC_MEMORY_CONTRACT,
                    where != NULL ? where : "unknown",
                    "memory contract");
    }
    return st;
}
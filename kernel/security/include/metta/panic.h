#ifndef METTA_PANIC_H
#define METTA_PANIC_H

#include "metta/types.h"

/*
 * Política de panic (memory-ownership.md §10, contracts.md §1).
 * Violaciones de contrato kernel → detener; errores de puntero usuario
 * en syscall se devuelven como metta_status_t sin panic.
 */

typedef enum {
    METTA_PANIC_BOOT = 0,
    METTA_PANIC_MEMORY_CONTRACT,
    METTA_PANIC_SECURITY_INTERNAL,
    METTA_PANIC_SUBSYSTEM
} metta_panic_class_t;

typedef enum {
    METTA_PANIC_ACT_HALT = 0,
    METTA_PANIC_ACT_LOG_HALT
} metta_panic_action_t;

typedef void (*metta_panic_diag_fn)(metta_panic_class_t cls,
                                    metta_status_t     st,
                                    const char        *where,
                                    const char        *detail);

void metta_panic_set_action(metta_panic_action_t action);
metta_panic_action_t metta_panic_get_action(void);

void metta_panic_register_diag(metta_panic_diag_fn fn);

__attribute__((noreturn)) void metta_panic(metta_panic_class_t cls,
                                           const char           *where,
                                           const char           *detail);

__attribute__((noreturn)) void metta_panic_status(metta_panic_class_t cls,
                                                    metta_status_t       st,
                                                    const char          *where,
                                                    const char          *detail);

/* Si st == METTA_ERR_CORRUPT → panic; si no, devuelve st. */
metta_status_t metta_panic_on_corrupt(metta_status_t st, const char *where);

#endif /* METTA_PANIC_H */
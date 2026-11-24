#ifndef METTA_VALIDATE_H
#define METTA_VALIDATE_H

#include "metta/types.h"

/*
 * Validación de punteros y rangos de espacio de usuario (contracts.md §4,
 * memory-ownership.md §5). No accede a la memoria usuario; comprueba
 * límites lógicos y, si hay sonda VMM, permisos de mapeo.
 */

/** Inicio mapeable usuario (página NULL sin mapear). */
#define METTA_USER_VIRT_MIN  0x0000000000400000ULL
/** Tope canónico usuario x86_64 (47 bits). */
#define METTA_USER_VIRT_MAX  0x00007FFFFFFFFFFFULL

typedef metta_u32 metta_user_access_t;

#define METTA_USER_ACCESS_READ  (1u << 0)
#define METTA_USER_ACCESS_WRITE (1u << 1)
#define METTA_USER_ACCESS_EXEC  (1u << 2)

/**
 * Permisos requeridos en sonda VMM (bits METTA_PERM_* del contrato VMM).
 * Valor 0 = solo comprobación de rango (sin sonda o sonda omitida).
 */
typedef struct {
    metta_virt_addr_t virt_start;
    metta_virt_addr_t virt_end; /* exclusivo */
    metta_u64         length;
} metta_user_range_t;

typedef metta_status_t (*metta_sec_vmm_probe_fn)(metta_virt_addr_t virt,
                                                   metta_u64         length,
                                                   metta_user_access_t access);

typedef struct {
    metta_virt_addr_t user_stack_top; /* 0 = sin límite stack (solo boot/tests) */
} metta_sec_context_t;

void metta_sec_init(void);
void metta_sec_set_context(const metta_sec_context_t *ctx);
void metta_sec_set_vmm_probe(metta_sec_vmm_probe_fn fn);

bool metta_sec_is_user_canonical(metta_virt_addr_t addr);
bool metta_sec_is_kernel_virt(metta_virt_addr_t addr);

metta_status_t metta_sec_validate_user_ptr(const void *ptr);
metta_status_t metta_sec_validate_user_range(const void *ptr, metta_u64 len);
metta_status_t metta_sec_validate_user_range_access(const void *ptr,
                                                    metta_u64     len,
                                                    metta_user_access_t access);

/* Reserva rango usuario [ptr, ptr+max_scan); sin leer bytes (evita fault directo). */
metta_status_t metta_sec_validate_user_cstr(const char *ptr, metta_u64 max_scan);

metta_status_t metta_sec_fill_user_range(const void *ptr, metta_u64 len,
                                         metta_user_range_t *out);

#endif /* METTA_VALIDATE_H */
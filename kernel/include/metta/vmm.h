#ifndef METTA_VMM_H
#define METTA_VMM_H

#include "metta/types.h"
#include "metta/pmm.h"

typedef struct metta_address_space metta_address_space_t;
typedef metta_u32 metta_asid_t;

#define METTA_ASID_KERNEL  0u
#define METTA_ASID_INVALID UINT32_MAX

typedef enum {
    METTA_PERM_NONE  = 0,
    METTA_PERM_READ  = 1u << 0,
    METTA_PERM_WRITE = 1u << 1,
    METTA_PERM_EXEC  = 1u << 2,
    METTA_PERM_USER  = 1u << 3
} metta_page_perm_t;

typedef struct {
    metta_virt_addr_t virt;
    metta_phys_frame_t *phys;
    metta_page_perm_t perms;
    metta_u64         flags;
} metta_map_request_t;

#define METTA_MAP_FIXED   (1u << 0)
#define METTA_MAP_NOEXEC  (1u << 1)

metta_status_t vmm_init(void);
metta_status_t vmm_late_init_after_pmm(void);

/** Mapea [phys, phys+len) 1:1 con páginas de 2 MiB (FB MMIO, etc.). */
metta_status_t vmm_map_identity_range(metta_phys_addr_t phys, metta_u64 length);

/* Marca páginas de 2 MiB ya identidad-mapeadas como accesibles desde ring 3. */
metta_status_t vmm_allow_user(metta_phys_addr_t phys, metta_u64 length);

metta_status_t vmm_as_create(metta_address_space_t **out, metta_asid_t *asid_out);
metta_status_t vmm_as_destroy(metta_address_space_t *as);

metta_status_t vmm_map(metta_address_space_t *as, const metta_map_request_t *req);
metta_status_t vmm_unmap(metta_address_space_t *as, metta_virt_addr_t virt, metta_u64 length);
metta_status_t vmm_protect(metta_address_space_t *as, metta_virt_addr_t virt,
                           metta_u64 length, metta_page_perm_t perms);

metta_status_t vmm_switch_to(metta_address_space_t *as);
metta_address_space_t *vmm_current_as(void);
metta_phys_addr_t vmm_virt_to_phys(metta_address_space_t *as, metta_virt_addr_t virt);

#endif /* METTA_VMM_H */
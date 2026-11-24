#ifndef METTA_TYPES_H
#define METTA_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define METTA_PAGE_SIZE          4096u
#define METTA_PAGE_SHIFT         12
#define METTA_CONTRACT_REV       1u

typedef uint8_t   metta_u8;
typedef uint16_t  metta_u16;
typedef int32_t   metta_s32;
typedef uint32_t  metta_u32;
typedef uint64_t  metta_u64;
typedef intptr_t  metta_ssize_t;
typedef uintptr_t metta_phys_addr_t;
typedef uintptr_t metta_virt_addr_t;

typedef struct {
    metta_phys_addr_t base;
    metta_u64         length;
    metta_u32         type;
    metta_u32         reserved;
} metta_mem_region_t;

/* Tipos de región (boot-map.md / Multiboot2 mmap) */
#define METTA_MEM_AVAILABLE        1u
#define METTA_MEM_RESERVED         2u
#define METTA_MEM_ACPI_RECLAIMABLE 3u
#define METTA_MEM_NVS              4u
#define METTA_MEM_BAD              5u
#define METTA_MEM_KERNEL_IMAGE     6u
#define METTA_MEM_FRAMEBUFFER      7u

typedef enum {
    METTA_OK = 0,
    METTA_ERR_INVALID_ARG,
    METTA_ERR_NO_MEMORY,
    METTA_ERR_NOT_FOUND,
    METTA_ERR_BUSY,
    METTA_ERR_PERM,
    METTA_ERR_UNINITIALIZED,
    METTA_ERR_CORRUPT,
    METTA_ERR_RANGE,
    METTA_ERR_INTERNAL
} metta_status_t;

#define METTA_PHYS_NONE  ((metta_phys_addr_t)0)
#define METTA_VIRT_NONE  ((metta_virt_addr_t)0)

/* HHDM — kernel virt base (boot-map.md rev 1) */
#define METTA_KERNEL_VIRT_BASE   0xFFFF800000000000ULL

#endif /* METTA_TYPES_H */
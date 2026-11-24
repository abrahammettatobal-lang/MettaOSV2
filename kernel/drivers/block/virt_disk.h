#ifndef METTA_VIRT_DISK_H
#define METTA_VIRT_DISK_H

#include <stdbool.h>
#include <stdint.h>

#include "block.h"
#include "metta/types.h"

/** RAM disk por defecto en QEMU (16 MiB). */
#define VIRT_DISK_DEFAULT_BYTES   (16u * 1024u * 1024u)

typedef enum {
    VIRT_DISK_BACKEND_RAM = 0,
    VIRT_DISK_BACKEND_IMAGE
} virt_disk_backend_t;

typedef struct {
    virt_disk_backend_t backend;
    void               *storage;
    uint64_t            byte_length;
    bool                image_writable;
} virt_disk_params_t;

typedef struct virt_disk {
    metta_block_dev_t    bdev;
    virt_disk_params_t   params;
} virt_disk_t;

/**
 * Disco RAM sobre un buffer ya reservado (estático o marcos PMM mapeados).
 * El buffer debe permanecer válido hasta virt_disk_teardown().
 */
metta_status_t virt_disk_attach_ram(virt_disk_t   *vd,
                                    const char    *name,
                                    void          *backing,
                                    uint64_t       byte_length);

/**
 * Disco respaldado por imagen lineal en memoria (p. ej. blob enlazado o mmap futuro).
 * Si @p image_writable es false, las escrituras devuelven METTA_ERR_PERM.
 */
metta_status_t virt_disk_attach_image(virt_disk_t *vd,
                                      const char  *name,
                                      void        *image,
                                      uint64_t     byte_length,
                                      bool         image_writable);

/** Instancia estática con buffer interno VIRT_DISK_DEFAULT_BYTES y nombre "vd0". */
metta_status_t virt_disk_create_default(virt_disk_t *vd);

/** Registra @p vd->bdev en la capa block. */
metta_status_t virt_disk_register(virt_disk_t *vd);

void virt_disk_teardown(virt_disk_t *vd);

/** Buffer y tamaño del disco por defecto (solo tras virt_disk_create_default). */
void *virt_disk_default_backing(uint64_t *out_bytes);

#endif /* METTA_VIRT_DISK_H */
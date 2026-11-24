/* MettaOS — disco virtual RAM / imagen (SA-25). */
#include "virt_disk.h"

#include <stddef.h>
#include <stdint.h>

static uint8_t g_default_ram[VIRT_DISK_DEFAULT_BYTES];

static void *vd_storage(const virt_disk_t *vd)
{
    return vd->params.storage;
}

static uint64_t vd_bytes(const virt_disk_t *vd)
{
    return vd->params.byte_length;
}

static metta_status_t vd_rw(void               *base,
                            uint64_t            nbytes,
                            metta_blk_lba_t     lba,
                            void               *buf,
                            uint32_t            count,
                            int                 is_write)
{
    uint64_t offset = (uint64_t)lba * (uint64_t)METTA_BLK_SECTOR_SIZE;
    uint64_t xfer   = (uint64_t)count * (uint64_t)METTA_BLK_SECTOR_SIZE;

    if (buf == NULL || count == 0u) {
        return METTA_ERR_INVALID_ARG;
    }
    if (offset + xfer > nbytes) {
        return METTA_ERR_RANGE;
    }

    uint8_t *dst = (uint8_t *)base + offset;
    uint8_t *src = (uint8_t *)buf;

    if (is_write) {
        for (uint64_t i = 0; i < xfer; i++) {
            dst[i] = src[i];
        }
    } else {
        for (uint64_t i = 0; i < xfer; i++) {
            src[i] = dst[i];
        }
    }
    return METTA_OK;
}

static metta_status_t virt_disk_read(metta_block_dev_t *bdev,
                                     metta_blk_lba_t    lba,
                                     void              *buf,
                                     uint32_t           count)
{
    virt_disk_t *vd = (virt_disk_t *)bdev->driver_data;
    if (vd == NULL) {
        return METTA_ERR_INTERNAL;
    }
    void *stor = vd_storage(vd);
    if (stor == NULL) {
        return METTA_ERR_UNINITIALIZED;
    }
    return vd_rw(stor, vd_bytes(vd), lba, buf, count, 0);
}

static metta_status_t virt_disk_write(metta_block_dev_t *bdev,
                                      metta_blk_lba_t    lba,
                                      void              *buf,
                                      uint32_t           count)
{
    virt_disk_t *vd = (virt_disk_t *)bdev->driver_data;
    if (vd == NULL) {
        return METTA_ERR_INTERNAL;
    }
    if (vd->params.backend == VIRT_DISK_BACKEND_IMAGE &&
        !vd->params.image_writable) {
        return METTA_ERR_PERM;
    }
    void *stor = vd_storage(vd);
    if (stor == NULL) {
        return METTA_ERR_UNINITIALIZED;
    }
    return vd_rw(stor, vd_bytes(vd), lba, buf, count, 1);
}

static metta_status_t virt_disk_setup(virt_disk_t            *vd,
                                      const char             *name,
                                      const virt_disk_params_t *params)
{
    if (vd == NULL || name == NULL || params == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (params->storage == NULL || params->byte_length < METTA_BLK_SECTOR_SIZE) {
        return METTA_ERR_INVALID_ARG;
    }
    if ((params->byte_length % METTA_BLK_SECTOR_SIZE) != 0u) {
        return METTA_ERR_INVALID_ARG;
    }

    vd->params = *params;
    vd->bdev.name          = name;
    vd->bdev.sector_count  = params->byte_length / METTA_BLK_SECTOR_SIZE;
    vd->bdev.read_sectors  = virt_disk_read;
    vd->bdev.write_sectors = virt_disk_write;
    vd->bdev.driver_data   = vd;
    return METTA_OK;
}

metta_status_t virt_disk_attach_ram(virt_disk_t   *vd,
                                    const char    *name,
                                    void          *backing,
                                    uint64_t       byte_length)
{
    virt_disk_params_t p = {
        .backend         = VIRT_DISK_BACKEND_RAM,
        .storage         = backing,
        .byte_length     = byte_length,
        .image_writable  = true,
    };
    return virt_disk_setup(vd, name, &p);
}

metta_status_t virt_disk_attach_image(virt_disk_t *vd,
                                      const char  *name,
                                      void        *image,
                                      uint64_t     byte_length,
                                      bool         image_writable)
{
    virt_disk_params_t p = {
        .backend         = VIRT_DISK_BACKEND_IMAGE,
        .storage         = image,
        .byte_length     = byte_length,
        .image_writable  = image_writable,
    };
    return virt_disk_setup(vd, name, &p);
}

metta_status_t virt_disk_create_default(virt_disk_t *vd)
{
    return virt_disk_attach_ram(vd, "vd0", g_default_ram, (uint64_t)sizeof(g_default_ram));
}

metta_status_t virt_disk_register(virt_disk_t *vd)
{
    if (vd == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    (void)block_init();
    return block_register(&vd->bdev);
}

void virt_disk_teardown(virt_disk_t *vd)
{
    if (vd == NULL || vd->bdev.name == NULL) {
        return;
    }
    (void)block_unregister(vd->bdev.name);
    vd->params.storage     = NULL;
    vd->params.byte_length = 0;
    vd->bdev.sector_count  = 0;
    vd->bdev.name          = NULL;
    vd->bdev.driver_data   = NULL;
}

void *virt_disk_default_backing(uint64_t *out_bytes)
{
    if (out_bytes != NULL) {
        *out_bytes = (uint64_t)sizeof(g_default_ram);
    }
    return g_default_ram;
}
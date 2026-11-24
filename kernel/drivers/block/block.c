/* MettaOS — capa de bloques genérica (SA-25). */
#include "block.h"

#include <stddef.h>
#include <stdint.h>

static metta_block_dev_t *g_block_table[METTA_BLK_MAX_DEVICES];
static uint32_t           g_block_count;
static int                g_block_inited;

static metta_status_t block_validate_io(const metta_block_dev_t *dev,
                                        metta_blk_lba_t          lba,
                                        uint32_t                 count)
{
    if (dev == NULL || count == 0u) {
        return METTA_ERR_INVALID_ARG;
    }
    if (lba >= dev->sector_count) {
        return METTA_ERR_RANGE;
    }
    if ((uint64_t)lba + (uint64_t)count > dev->sector_count) {
        return METTA_ERR_RANGE;
    }
    return METTA_OK;
}

metta_status_t block_init(void)
{
    if (g_block_inited) {
        return METTA_OK;
    }
    for (uint32_t i = 0; i < METTA_BLK_MAX_DEVICES; i++) {
        g_block_table[i] = NULL;
    }
    g_block_count   = 0;
    g_block_inited  = 1;
    return METTA_OK;
}

metta_status_t block_register(metta_block_dev_t *dev)
{
    if (!g_block_inited) {
        (void)block_init();
    }
    if (dev == NULL || dev->name == NULL || dev->sector_count == 0u ||
        dev->read_sectors == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (block_lookup(dev->name) != NULL) {
        return METTA_ERR_BUSY;
    }
    if (g_block_count >= METTA_BLK_MAX_DEVICES) {
        return METTA_ERR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < METTA_BLK_MAX_DEVICES; i++) {
        if (g_block_table[i] == NULL) {
            g_block_table[i] = dev;
            g_block_count++;
            return METTA_OK;
        }
    }
    return METTA_ERR_INTERNAL;
}

metta_status_t block_unregister(const char *name)
{
    if (name == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (!g_block_inited) {
        return METTA_ERR_NOT_FOUND;
    }
    for (uint32_t i = 0; i < METTA_BLK_MAX_DEVICES; i++) {
        if (g_block_table[i] != NULL && g_block_table[i]->name != NULL) {
            const char *a = g_block_table[i]->name;
            const char *b = name;
            int same = 1;
            while (*a != '\0' || *b != '\0') {
                if (*a != *b) {
                    same = 0;
                    break;
                }
                a++;
                b++;
            }
            if (same) {
                g_block_table[i] = NULL;
                if (g_block_count > 0u) {
                    g_block_count--;
                }
                return METTA_OK;
            }
        }
    }
    return METTA_ERR_NOT_FOUND;
}

metta_block_dev_t *block_lookup(const char *name)
{
    if (name == NULL || !g_block_inited) {
        return NULL;
    }
    for (uint32_t i = 0; i < METTA_BLK_MAX_DEVICES; i++) {
        metta_block_dev_t *dev = g_block_table[i];
        if (dev == NULL || dev->name == NULL) {
            continue;
        }
        const char *a = dev->name;
        const char *b = name;
        int same = 1;
        while (*a != '\0' || *b != '\0') {
            if (*a != *b) {
                same = 0;
                break;
            }
            a++;
            b++;
        }
        if (same) {
            return dev;
        }
    }
    return NULL;
}

metta_status_t block_read(metta_block_dev_t *dev,
                          metta_blk_lba_t    lba,
                          void              *buf,
                          uint32_t           count)
{
    metta_status_t st = block_validate_io(dev, lba, count);
    if (st != METTA_OK) {
        return st;
    }
    if (buf == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    return dev->read_sectors(dev, lba, buf, count);
}

metta_status_t block_write(metta_block_dev_t *dev,
                           metta_blk_lba_t    lba,
                           const void        *buf,
                           uint32_t           count)
{
    metta_status_t st = block_validate_io(dev, lba, count);
    if (st != METTA_OK) {
        return st;
    }
    if (buf == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (dev->write_sectors == NULL) {
        return METTA_ERR_PERM;
    }
    return dev->write_sectors(dev, lba, (void *)buf, count);
}
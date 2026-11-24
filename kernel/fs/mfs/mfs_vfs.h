#ifndef MFS_VFS_H
#define MFS_VFS_H

#include <fs/vfs.h>

#include "../../drivers/block/block.h"

/** Tabla de operaciones registrada en `vfs_mount`. */
extern vfs_fs_ops_t mfs_vfs_ops;

/**
 * Monta MFS sobre @p bdev en @p mount_path (p. ej. `"/"`).
 * Formatea automáticamente si el volumen no tiene firma MFS válida.
 */
metta_status_t mfs_vfs_mount_volume(metta_block_dev_t *bdev, const char *mount_path);

/** Atajo: `block_lookup("vd0")` + `mfs_vfs_mount_volume`. */
metta_status_t mfs_vfs_mount_vd0(const char *mount_path);

/**
 * Stub para `kmain`: monta MFS en `"/"` sobre `vd0`.
 * Precondiciones: `vfs_init()`, `block_init()`, `virt_disk_register` (vd0 registrado).
 */
metta_status_t mfs_vfs_kmain_stub(void);

#endif /* MFS_VFS_H */
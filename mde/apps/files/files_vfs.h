#ifndef MDE_FILES_VFS_H
#define MDE_FILES_VFS_H

#include <stddef.h>
#include <stdint.h>

/*
 * Puerto VFS ↔ MDE Files (SA-26).
 * La implementación real vive en el subsistema VFS del kernel (misión futura);
 * hasta entonces el stub en files_vfs_stub.c sirve datos de demostración.
 */

#define MDE_FILES_PATH_MAX   384
#define MDE_FILES_NAME_MAX   255

#define MDE_FILES_ENT_DIR    (1u << 0)
#define MDE_FILES_ENT_SYM    (1u << 1)

typedef enum {
    MDE_FILES_VFS_OK = 0,
    MDE_FILES_VFS_NOT_FOUND,
    MDE_FILES_VFS_NOT_DIR,
    MDE_FILES_VFS_IO,
    MDE_FILES_VFS_NO_MEMORY,
    MDE_FILES_VFS_INVALID
} mde_files_vfs_status;

typedef struct {
    char     name[MDE_FILES_NAME_MAX + 1];
    uint32_t flags;
    uint64_t size_bytes;
    uint64_t mtime_unix; /* 0 = desconocido */
} mde_files_dirent_t;

typedef struct mde_files_vfs_ops {
    void *ctx;

    mde_files_vfs_status (*resolve)(void *ctx, const char *path, mde_files_dirent_t *out);
    mde_files_vfs_status (*read_dir)(
        void *ctx,
        const char *path,
        mde_files_dirent_t *entries,
        size_t           cap,
        size_t          *out_count);
    mde_files_vfs_status (*parent_path)(void *ctx, const char *path, char *parent, size_t parent_cap);
} mde_files_vfs_ops;

typedef struct {
    const mde_files_vfs_ops *ops;
    int                      initialized;
} mde_files_vfs;

mde_files_vfs_status mde_files_vfs_bind_stub(mde_files_vfs *vfs);
mde_files_vfs_status mde_files_vfs_bind(mde_files_vfs *vfs, const mde_files_vfs_ops *ops);

/* Normaliza separadores a '/'; garantiza path absoluto con raíz "/". */
void mde_files_path_normalize(char *path, size_t cap);

int mde_files_path_is_child(const char *parent, const char *child);

#endif /* MDE_FILES_VFS_H */
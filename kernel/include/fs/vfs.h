/*
 * MettaOS — Virtual File System (SA-23)
 * Contrato VFS↔MFS: `vfs_fs_ops_t` / `vfs_node_t`
 * Contrato VFS↔MDE Files: solo esta API kernel (syscall futura `metta_open`)
 */
#ifndef METTA_FS_VFS_H
#define METTA_FS_VFS_H

#include <stddef.h>
#include <stdint.h>

#include "metta/types.h"

#define VFS_PATH_MAX    256u
#define VFS_NAME_MAX    64u
#define VFS_MAX_MOUNTS  4u
#define VFS_MAX_FDS     64u

/* Modos de apertura (combinables donde tenga sentido) */
#define VFS_O_RDONLY    (1u << 0)
#define VFS_O_WRONLY    (1u << 1)
#define VFS_O_RDWR      (VFS_O_RDONLY | VFS_O_WRONLY)
#define VFS_O_CREAT     (1u << 2)
#define VFS_O_TRUNC     (1u << 3)
#define VFS_O_DIRECTORY (1u << 4)
#define VFS_O_APPEND    (1u << 5)
#define VFS_O_EXCL      (1u << 6)

#define VFS_O_ACCMODE   (VFS_O_RDONLY | VFS_O_WRONLY | VFS_O_RDWR)

/* Códigos negativos para capa syscall / MDE (mapeo desde metta_status_t) */
#define VFS_ENOENT    (-2)
#define VFS_EIO       (-5)
#define VFS_EBADF     (-9)
#define VFS_EINVAL    (-22)
#define VFS_EEXIST    (-17)
#define VFS_ENOSPC    (-28)
#define VFS_ENOTDIR   (-20)
#define VFS_EISDIR    (-21)
#define VFS_ENOTEMPTY (-39)

typedef enum {
    VFS_NODE_UNKNOWN = 0,
    VFS_NODE_FILE,
    VFS_NODE_DIR,
} vfs_node_type_t;

typedef struct vfs_mount vfs_mount_t;
typedef struct vfs_node  vfs_node_t;

typedef struct {
    char            name[VFS_NAME_MAX];
    vfs_node_type_t type;
    metta_u64       size;
    metta_u32       mode;
} vfs_dirent_t;

/*
 * Tabla que MFS (SA-24) registra por volumen montado.
 * Paths pasados al backend son relativos al punto de montaje, sin '/' inicial
 * (cadena vacía = raíz del volumen).
 */
typedef struct vfs_fs_ops {
    const char *fs_name;

    metta_status_t (*lookup)(vfs_mount_t *mnt, const char *rel_path, vfs_node_t **out);
    metta_status_t (*create)(vfs_mount_t *mnt, const char *rel_path, vfs_node_type_t type);
    metta_status_t (*mkdir)(vfs_mount_t *mnt, const char *rel_path);

    metta_status_t (*open_node)(vfs_mount_t *mnt, vfs_node_t *node, metta_u32 flags,
                                void **handle_out);
    void (*close_node)(vfs_mount_t *mnt, void *handle);

    metta_ssize_t (*read)(vfs_mount_t *mnt, void *handle, void *buf, metta_u64 len,
                          metta_u64 *offset_io);
    metta_ssize_t (*write)(vfs_mount_t *mnt, void *handle, const void *buf, metta_u64 len,
                           metta_u64 *offset_io);

    metta_status_t (*readdir)(vfs_mount_t *mnt, void *dir_handle, vfs_dirent_t *ent, int *more);

    void (*release_node)(vfs_mount_t *mnt, vfs_node_t *node);
} vfs_fs_ops_t;

struct vfs_mount {
    char         mount_path[VFS_PATH_MAX];
    metta_u32    mount_path_len;
    vfs_fs_ops_t *ops;
    void         *fs_private;
};

struct vfs_node {
    vfs_node_type_t type;
    void           *fs_cookie;
    vfs_mount_t    *mount;
    metta_u64       size;
};

typedef struct {
    int            in_use;
    metta_u32      flags;
    metta_u64      offset;
    vfs_node_t    *node;
    void          *fs_handle;
    vfs_mount_t   *mount;
    int            is_dir;
} vfs_fd_slot_t;

metta_status_t vfs_init(void);
metta_status_t vfs_mount(vfs_fs_ops_t *ops, const char *mount_path, void *fs_private);
metta_status_t vfs_unmount(const char *mount_path);

metta_status_t vfs_open(const char *path, metta_u32 flags, int *out_fd);
metta_status_t vfs_close(int fd);
metta_ssize_t  vfs_read(int fd, void *buf, metta_u64 len);
metta_status_t vfs_seek(int fd, metta_u64 offset);
metta_u64      vfs_fd_size(int fd);
metta_ssize_t  vfs_write(int fd, const void *buf, metta_u64 len);
metta_status_t vfs_mkdir(const char *path);
metta_status_t vfs_readdir(int fd, vfs_dirent_t *ent, int *more);

vfs_mount_t *vfs_resolve_mount(const char *path);
int          vfs_errno_from_status(metta_status_t st);

#endif /* METTA_FS_VFS_H */
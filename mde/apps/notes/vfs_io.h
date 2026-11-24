#ifndef MDE_NOTES_VFS_IO_H
#define MDE_NOTES_VFS_IO_H

#include "notes.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Adaptador hacia VFS de MettaOS (SA-23).
 * La app de notas no llama al kernel directamente: registra operaciones inyectadas.
 */

typedef struct notes_vfs_file *notes_vfs_handle_t;

typedef struct notes_vfs_ops {
    notes_status_t (*open)(const char *path, int flags, notes_vfs_handle_t *out);
    notes_status_t (*close)(notes_vfs_handle_t h);
    notes_status_t (*read)(notes_vfs_handle_t h, void *buf, size_t len, size_t *out_read);
    notes_status_t (*write)(notes_vfs_handle_t h, const void *buf, size_t len, size_t *out_written);
    notes_status_t (*truncate)(notes_vfs_handle_t h, size_t new_size);
} notes_vfs_ops_t;

/* flags compatibles con SA-23 (subset) */
#define NOTES_VFS_O_RDONLY  0x0001
#define NOTES_VFS_O_WRONLY  0x0002
#define NOTES_VFS_O_RDWR    0x0003
#define NOTES_VFS_O_CREAT   0x0100
#define NOTES_VFS_O_TRUNC   0x0200

void notes_vfs_bind(const notes_vfs_ops_t *ops);
bool notes_vfs_is_bound(void);

notes_status_t notes_vfs_read_file(const char *path, char *buf, size_t buf_cap, size_t *out_len);
notes_status_t notes_vfs_write_file(const char *path, const char *data, size_t len);

#endif /* MDE_NOTES_VFS_IO_H */
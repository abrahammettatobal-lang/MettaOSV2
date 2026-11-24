/* MDE Notes — E/S vía VFS inyectada (SA-27 / contrato SA-23) */

#include "vfs_io.h"

static const notes_vfs_ops_t *g_vfs_ops;

void notes_vfs_bind(const notes_vfs_ops_t *ops)
{
    g_vfs_ops = ops;
}

bool notes_vfs_is_bound(void)
{
    return g_vfs_ops != NULL && g_vfs_ops->open != NULL && g_vfs_ops->close != NULL &&
           g_vfs_ops->read != NULL && g_vfs_ops->write != NULL;
}

static size_t path_length(const char *path)
{
    size_t n = 0;
    if (path == NULL) {
        return 0;
    }
    while (path[n] != '\0' && n < MDE_NOTES_MAX_PATH) {
        n++;
    }
    return n;
}

notes_status_t notes_vfs_read_file(const char *path, char *buf, size_t buf_cap, size_t *out_len)
{
    notes_vfs_handle_t h;
    size_t total = 0;
    notes_status_t st;

    if (buf == NULL || out_len == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (path == NULL || path_length(path) == 0u) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (!notes_vfs_is_bound()) {
        return NOTES_ERR_VFS_UNBOUND;
    }

    st = g_vfs_ops->open(path, NOTES_VFS_O_RDONLY, &h);
    if (st != NOTES_OK) {
        return st;
    }

    while (total < buf_cap) {
        size_t chunk = 0;
        st           = g_vfs_ops->read(h, buf + total, buf_cap - total, &chunk);
        if (st != NOTES_OK) {
            g_vfs_ops->close(h);
            return st;
        }
        if (chunk == 0u) {
            break;
        }
        total += chunk;
    }

    if (total >= buf_cap) {
        g_vfs_ops->close(h);
        return NOTES_ERR_TOO_LARGE;
    }

    buf[total] = '\0';
    g_vfs_ops->close(h);
    *out_len   = total;
    return NOTES_OK;
}

notes_status_t notes_vfs_write_file(const char *path, const char *data, size_t len)
{
    notes_vfs_handle_t h;
    size_t written = 0;
    notes_status_t st;

    if (data == NULL && len != 0u) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (path == NULL || path_length(path) == 0u) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (!notes_vfs_is_bound()) {
        return NOTES_ERR_VFS_UNBOUND;
    }

    st = g_vfs_ops->open(path, NOTES_VFS_O_WRONLY | NOTES_VFS_O_CREAT | NOTES_VFS_O_TRUNC, &h);
    if (st != NOTES_OK) {
        return st;
    }

    if (g_vfs_ops->truncate != NULL) {
        (void)g_vfs_ops->truncate(h, len);
    }

    st = g_vfs_ops->write(h, data, len, &written);
    if (st != NOTES_OK || written != len) {
        g_vfs_ops->close(h);
        return (st != NOTES_OK) ? st : NOTES_ERR_IO;
    }

    st = g_vfs_ops->close(h);
    return st;
}
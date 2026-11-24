/*
 * MettaOS — VFS (SA-23): descriptores, rutas POSIX y delegación a backends.
 */
#include <fs/vfs.h>

static int           vfs_ready;
static vfs_mount_t     vfs_mounts[VFS_MAX_MOUNTS];
static metta_u32     vfs_mount_count;
static vfs_fd_slot_t vfs_fds[VFS_MAX_FDS];

static int vfs_is_slash(char c)
{
    return c == '/';
}

static metta_u32 vfs_strlen(const char *s)
{
    metta_u32 n = 0;
    if (s == NULL) {
        return 0;
    }
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int vfs_streq(const char *a, const char *b)
{
    metta_u32 i = 0;
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

/*
 * Normaliza a ruta absoluta en out (cap out_cap incluye NUL).
 * Colapsa '//', elimina '/./' y resuelve '/../'.
 */
static metta_status_t vfs_normalize(const char *path, char *out, metta_u32 out_cap)
{
    const char *walk;
    char       *dst;
    metta_u32   len;
    char        stack[VFS_PATH_MAX][VFS_NAME_MAX];
    metta_u32   depth = 0;

    if (path == NULL || out == NULL || out_cap < 2u) {
        return METTA_ERR_INVALID_ARG;
    }

    while (vfs_is_slash(*path)) {
        path++;
    }
    if (*path == '\0') {
        out[0] = '/';
        out[1] = '\0';
        return METTA_OK;
    }

    walk = path;
    while (*walk != '\0') {
        while (vfs_is_slash(*walk)) {
            walk++;
        }
        if (*walk == '\0') {
            break;
        }

        len = 0;
        while (walk[len] != '\0' && !vfs_is_slash(walk[len])) {
            if (len + 1u >= VFS_NAME_MAX) {
                return METTA_ERR_RANGE;
            }
            stack[depth][len] = walk[len];
            len++;
        }
        stack[depth][len] = '\0';
        walk += len;

        if (vfs_streq(stack[depth], ".")) {
            continue;
        }
        if (vfs_streq(stack[depth], "..")) {
            if (depth > 0u) {
                depth--;
            }
            continue;
        }
        depth++;
        if (depth >= VFS_PATH_MAX) {
            return METTA_ERR_RANGE;
        }
    }

    dst = out;
    if (depth == 0u) {
        dst[0] = '/';
        dst[1] = '\0';
        return METTA_OK;
    }

    *dst++ = '/';
    if ((metta_u32)(dst - out) >= out_cap) {
        return METTA_ERR_RANGE;
    }

    for (metta_u32 i = 0; i < depth; i++) {
        const char *seg = stack[i];
        metta_u32   sl  = vfs_strlen(seg);
        metta_u32   used = (metta_u32)(dst - out);

        if (used + sl + 2u >= out_cap) {
            return METTA_ERR_RANGE;
        }
        for (metta_u32 j = 0; j < sl; j++) {
            *dst++ = seg[j];
        }
        if (i + 1u < depth) {
            *dst++ = '/';
        }
    }
    *dst = '\0';
    return METTA_OK;
}

static metta_status_t vfs_split_relative(const char *norm_path, const vfs_mount_t *mnt,
                                         char *rel_out, metta_u32 rel_cap)
{
    const char *suffix;
    metta_u32   mlen;
    metta_u32   i;

    if (norm_path == NULL || mnt == NULL || rel_out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    mlen = mnt->mount_path_len;
    if (mlen == 1u && mnt->mount_path[0] == '/') {
        suffix = norm_path;
        if (*suffix == '/') {
            suffix++;
        }
    } else {
        if (vfs_strlen(norm_path) < mlen) {
            return METTA_ERR_NOT_FOUND;
        }
        for (i = 0; i < mlen; i++) {
            if (norm_path[i] != mnt->mount_path[i]) {
                return METTA_ERR_NOT_FOUND;
            }
        }
        if (norm_path[mlen] != '\0' && !vfs_is_slash(norm_path[mlen])) {
            return METTA_ERR_NOT_FOUND;
        }
        suffix = norm_path + mlen;
        if (*suffix == '/') {
            suffix++;
        }
    }

    if (*suffix == '\0') {
        rel_out[0] = '\0';
        return METTA_OK;
    }

    i = 0;
    while (suffix[i] != '\0') {
        if (i + 1u >= rel_cap) {
            return METTA_ERR_RANGE;
        }
        rel_out[i] = suffix[i];
        i++;
    }
    rel_out[i] = '\0';
    return METTA_OK;
}

vfs_mount_t *vfs_resolve_mount(const char *path)
{
    char        norm[VFS_PATH_MAX];
    vfs_mount_t *best = NULL;
    metta_u32   best_len = 0;

    if (!vfs_ready || path == NULL) {
        return NULL;
    }
    if (vfs_normalize(path, norm, sizeof(norm)) != METTA_OK) {
        return NULL;
    }

    for (metta_u32 i = 0; i < vfs_mount_count; i++) {
        vfs_mount_t *m = &vfs_mounts[i];
        const char  *mp = m->mount_path;
        metta_u32    mlen = m->mount_path_len;
        int          match = 0;

        if (mlen == 0u || m->ops == NULL) {
            continue;
        }

        if (mlen == 1u && mp[0] == '/') {
            if (norm[0] == '/') {
                match = 1;
            }
        } else if (vfs_streq(norm, mp)) {
            match = 1;
        } else if (vfs_strlen(norm) > mlen) {
            metta_u32 j;
            for (j = 0; j < mlen; j++) {
                if (norm[j] != mp[j]) {
                    break;
                }
            }
            if (j == mlen && norm[mlen] == '/') {
                match = 1;
            }
        }

        if (match && mlen > best_len) {
            best = m;
            best_len = mlen;
        }
    }
    return best;
}

int vfs_errno_from_status(metta_status_t st)
{
    switch (st) {
    case METTA_OK:
        return 0;
    case METTA_ERR_NOT_FOUND:
        return VFS_ENOENT;
    case METTA_ERR_INVALID_ARG:
    case METTA_ERR_RANGE:
        return VFS_EINVAL;
    case METTA_ERR_PERM:
        return VFS_EIO;
    case METTA_ERR_BUSY:
        return VFS_EEXIST;
    default:
        return VFS_EIO;
    }
}

metta_status_t vfs_init(void)
{
    metta_u32 i;

    for (i = 0; i < VFS_MAX_MOUNTS; i++) {
        vfs_mounts[i].mount_path[0] = '\0';
        vfs_mounts[i].mount_path_len = 0;
        vfs_mounts[i].ops = NULL;
        vfs_mounts[i].fs_private = NULL;
    }
    vfs_mount_count = 0;

    for (i = 0; i < VFS_MAX_FDS; i++) {
        vfs_fds[i].in_use = 0;
        vfs_fds[i].node = NULL;
        vfs_fds[i].fs_handle = NULL;
        vfs_fds[i].mount = NULL;
    }

    vfs_ready = 1;
    return METTA_OK;
}

metta_status_t vfs_mount(vfs_fs_ops_t *ops, const char *mount_path, void *fs_private)
{
    char        norm[VFS_PATH_MAX];
    metta_u32   len;
    vfs_mount_t *m;

    if (!vfs_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (ops == NULL || mount_path == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (vfs_mount_count >= VFS_MAX_MOUNTS) {
        return METTA_ERR_NO_MEMORY;
    }

    if (vfs_normalize(mount_path, norm, sizeof(norm)) != METTA_OK) {
        return METTA_ERR_INVALID_ARG;
    }

    for (metta_u32 i = 0; i < vfs_mount_count; i++) {
        if (vfs_streq(vfs_mounts[i].mount_path, norm)) {
            return METTA_ERR_BUSY;
        }
    }

    m = &vfs_mounts[vfs_mount_count++];
    len = vfs_strlen(norm);
    if (len >= VFS_PATH_MAX) {
        vfs_mount_count--;
        return METTA_ERR_RANGE;
    }
    for (metta_u32 i = 0; i <= len; i++) {
        m->mount_path[i] = norm[i];
    }
    m->mount_path_len = len;
    m->ops = ops;
    m->fs_private = fs_private;
    return METTA_OK;
}

metta_status_t vfs_unmount(const char *mount_path)
{
    char norm[VFS_PATH_MAX];

    if (!vfs_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (mount_path == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (vfs_normalize(mount_path, norm, sizeof(norm)) != METTA_OK) {
        return METTA_ERR_INVALID_ARG;
    }

    for (metta_u32 i = 0; i < vfs_mount_count; i++) {
        if (!vfs_streq(vfs_mounts[i].mount_path, norm)) {
            continue;
        }
        for (metta_u32 f = 0; f < VFS_MAX_FDS; f++) {
            if (vfs_fds[f].in_use && vfs_fds[f].mount == &vfs_mounts[i]) {
                return METTA_ERR_BUSY;
            }
        }
        for (metta_u32 j = i + 1u; j < vfs_mount_count; j++) {
            vfs_mounts[j - 1u] = vfs_mounts[j];
        }
        vfs_mount_count--;
        return METTA_OK;
    }
    return METTA_ERR_NOT_FOUND;
}

static int vfs_alloc_fd(void)
{
    for (metta_u32 i = 0; i < VFS_MAX_FDS; i++) {
        if (!vfs_fds[i].in_use) {
            return (int)i;
        }
    }
    return -1;
}

static vfs_fd_slot_t *vfs_fd_get(int fd)
{
    if (fd < 0 || (metta_u32)fd >= VFS_MAX_FDS) {
        return NULL;
    }
    if (!vfs_fds[fd].in_use) {
        return NULL;
    }
    return &vfs_fds[fd];
}

metta_status_t vfs_open(const char *path, metta_u32 flags, int *out_fd)
{
    char           norm[VFS_PATH_MAX];
    char           rel[VFS_PATH_MAX];
    vfs_mount_t   *mnt;
    vfs_node_t    *node = NULL;
    vfs_fs_ops_t  *ops;
    void          *handle = NULL;
    metta_status_t st;
    int            fd;
    vfs_fd_slot_t *slot;

    if (!vfs_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (path == NULL || out_fd == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if ((flags & VFS_O_DIRECTORY) == 0u && (flags & VFS_O_ACCMODE) == 0u) {
        return METTA_ERR_INVALID_ARG;
    }

    st = vfs_normalize(path, norm, sizeof(norm));
    if (st != METTA_OK) {
        return st;
    }

    mnt = vfs_resolve_mount(norm);
    if (mnt == NULL || mnt->ops == NULL) {
        return METTA_ERR_NOT_FOUND;
    }
    ops = mnt->ops;

    st = vfs_split_relative(norm, mnt, rel, sizeof(rel));
    if (st != METTA_OK) {
        return st;
    }

    st = ops->lookup(mnt, rel, &node);
    if (st == METTA_ERR_NOT_FOUND && (flags & VFS_O_CREAT) != 0u) {
        vfs_node_type_t ntype =
            (flags & VFS_O_DIRECTORY) ? VFS_NODE_DIR : VFS_NODE_FILE;
        st = ops->create(mnt, rel, ntype);
        if (st != METTA_OK) {
            return st;
        }
        st = ops->lookup(mnt, rel, &node);
    }
    if (st != METTA_OK || node == NULL) {
        return st != METTA_OK ? st : METTA_ERR_NOT_FOUND;
    }

    if ((flags & VFS_O_DIRECTORY) != 0u && node->type != VFS_NODE_DIR) {
        if (ops->release_node != NULL) {
            ops->release_node(mnt, node);
        }
        return METTA_ERR_INVALID_ARG;
    }
    if ((flags & VFS_O_DIRECTORY) == 0u && node->type == VFS_NODE_DIR) {
        if (ops->release_node != NULL) {
            ops->release_node(mnt, node);
        }
        return METTA_ERR_PERM;
    }

    if (ops->open_node == NULL) {
        if (ops->release_node != NULL) {
            ops->release_node(mnt, node);
        }
        return METTA_ERR_INTERNAL;
    }

    st = ops->open_node(mnt, node, flags, &handle);
    if (st != METTA_OK) {
        if (ops->release_node != NULL) {
            ops->release_node(mnt, node);
        }
        return st;
    }

    fd = vfs_alloc_fd();
    if (fd < 0) {
        ops->close_node(mnt, handle);
        if (ops->release_node != NULL) {
            ops->release_node(mnt, node);
        }
        return METTA_ERR_NO_MEMORY;
    }

    slot = &vfs_fds[fd];
    slot->in_use = 1;
    slot->flags = flags;
    slot->offset = (flags & VFS_O_APPEND) ? node->type == VFS_NODE_FILE ? 0 : 0 : 0;
    slot->node = node;
    slot->fs_handle = handle;
    slot->mount = mnt;
    slot->is_dir = (node->type == VFS_NODE_DIR) ? 1 : 0;

    *out_fd = fd;
    return METTA_OK;
}

metta_status_t vfs_close(int fd)
{
    vfs_fd_slot_t *slot = vfs_fd_get(fd);
    vfs_fs_ops_t  *ops;

    if (!vfs_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (slot == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    ops = slot->mount->ops;
    if (ops != NULL && ops->close_node != NULL && slot->fs_handle != NULL) {
        ops->close_node(slot->mount, slot->fs_handle);
    }
    if (ops != NULL && ops->release_node != NULL && slot->node != NULL) {
        ops->release_node(slot->mount, slot->node);
    }

    slot->in_use = 0;
    slot->node = NULL;
    slot->fs_handle = NULL;
    slot->mount = NULL;
    slot->flags = 0;
    slot->offset = 0;
    slot->is_dir = 0;
    return METTA_OK;
}

metta_status_t vfs_seek(int fd, metta_u64 offset)
{
    vfs_fd_slot_t *slot = vfs_fd_get(fd);

    if (!vfs_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (slot == NULL || slot->is_dir) {
        return METTA_ERR_INVALID_ARG;
    }
    slot->offset = offset;
    return METTA_OK;
}

metta_u64 vfs_fd_size(int fd)
{
    vfs_fd_slot_t *slot = vfs_fd_get(fd);

    if (!vfs_ready || slot == NULL || slot->node == NULL) {
        return 0;
    }
    return slot->node->size;
}

metta_ssize_t vfs_read(int fd, void *buf, metta_u64 len)
{
    vfs_fd_slot_t *slot = vfs_fd_get(fd);
    vfs_fs_ops_t  *ops;
    metta_ssize_t  n;

    if (!vfs_ready) {
        return (metta_ssize_t)VFS_EIO;
    }
    if (slot == NULL || buf == NULL) {
        return (metta_ssize_t)VFS_EBADF;
    }
    if (slot->is_dir) {
        return (metta_ssize_t)VFS_EISDIR;
    }
    if ((slot->flags & VFS_O_RDONLY) == 0u && (slot->flags & VFS_O_RDWR) == 0u) {
        return (metta_ssize_t)VFS_EINVAL;
    }

    ops = slot->mount->ops;
    if (ops == NULL || ops->read == NULL) {
        return (metta_ssize_t)VFS_EIO;
    }

    n = ops->read(slot->mount, slot->fs_handle, buf, len, &slot->offset);
    if (n < 0) {
        return n;
    }
    return n;
}

metta_ssize_t vfs_write(int fd, const void *buf, metta_u64 len)
{
    vfs_fd_slot_t *slot = vfs_fd_get(fd);
    vfs_fs_ops_t  *ops;
    metta_ssize_t  n;

    if (!vfs_ready) {
        return (metta_ssize_t)VFS_EIO;
    }
    if (slot == NULL || buf == NULL) {
        return (metta_ssize_t)VFS_EBADF;
    }
    if (slot->is_dir) {
        return (metta_ssize_t)VFS_EISDIR;
    }
    if ((slot->flags & VFS_O_WRONLY) == 0u && (slot->flags & VFS_O_RDWR) == 0u) {
        return (metta_ssize_t)VFS_EINVAL;
    }

    ops = slot->mount->ops;
    if (ops == NULL || ops->write == NULL) {
        return (metta_ssize_t)VFS_EIO;
    }

    n = ops->write(slot->mount, slot->fs_handle, buf, len, &slot->offset);
    if (n < 0) {
        return n;
    }
    return n;
}

metta_status_t vfs_mkdir(const char *path)
{
    char           norm[VFS_PATH_MAX];
    char           rel[VFS_PATH_MAX];
    vfs_mount_t   *mnt;
    vfs_fs_ops_t  *ops;
    metta_status_t st;

    if (!vfs_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (path == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    st = vfs_normalize(path, norm, sizeof(norm));
    if (st != METTA_OK) {
        return st;
    }

    mnt = vfs_resolve_mount(norm);
    if (mnt == NULL || mnt->ops == NULL) {
        return METTA_ERR_NOT_FOUND;
    }
    ops = mnt->ops;
    if (ops->mkdir == NULL) {
        return METTA_ERR_INTERNAL;
    }

    st = vfs_split_relative(norm, mnt, rel, sizeof(rel));
    if (st != METTA_OK) {
        return st;
    }

    return ops->mkdir(mnt, rel);
}

metta_status_t vfs_readdir(int fd, vfs_dirent_t *ent, int *more)
{
    vfs_fd_slot_t *slot = vfs_fd_get(fd);
    vfs_fs_ops_t  *ops;

    if (!vfs_ready) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (ent == NULL || more == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (slot == NULL) {
        return METTA_ERR_INVALID_ARG;
    }
    if (!slot->is_dir) {
        return METTA_ERR_PERM;
    }

    ops = slot->mount->ops;
    if (ops == NULL || ops->readdir == NULL) {
        return METTA_ERR_INTERNAL;
    }

    return ops->readdir(slot->mount, slot->fs_handle, ent, more);
}
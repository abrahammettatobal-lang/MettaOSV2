#include "mfs_vfs.h"

#include "mfs.h"
#include "mfs_internal.h"
#include "blkdev_vd.h"

#include <stddef.h>
#include <stdint.h>

#define MFS_VFS_NODE_POOL    48u
#define MFS_VFS_HANDLE_POOL  48u
#define MFS_VFS_VD0_NAME      "vd0"

typedef struct mfs_vfs_volume {
    mfs_fs_t         fs;
    mfs_blkdev_t     blk;
    metta_block_dev_t *bdev;
    int              armed;
} mfs_vfs_volume_t;

typedef struct {
    uint8_t  used;
    uint8_t  is_dir;
    uint32_t ino;
    mfs_readdir_ctx_t dir_ctx;
} mfs_vfs_handle_t;

static mfs_vfs_volume_t g_vol;
static vfs_node_t       g_nodes[MFS_VFS_NODE_POOL];
static uint8_t          g_node_used[MFS_VFS_NODE_POOL];
static mfs_vfs_handle_t g_handles[MFS_VFS_HANDLE_POOL];

static metta_status_t mfs_vfs_mkdir(vfs_mount_t *mnt, const char *rel_path);

static metta_status_t mfs_status_to_metta(int st)
{
    switch (st) {
    case MFS_OK:
        return METTA_OK;
    case MFS_ENOENT:
        return METTA_ERR_NOT_FOUND;
    case MFS_EEXIST:
        return METTA_ERR_BUSY;
    case MFS_ENOSPC:
        return METTA_ERR_NO_MEMORY;
    case MFS_EINVAL:
    case MFS_ENOTDIR:
        return METTA_ERR_INVALID_ARG;
    case MFS_ECORRUPT:
        return METTA_ERR_CORRUPT;
    case MFS_EIO:
    default:
        return METTA_ERR_INTERNAL;
    }
}

static mfs_fs_t *mfs_vfs_fs(vfs_mount_t *mnt)
{
    mfs_vfs_volume_t *vol;

    if (mnt == NULL || mnt->fs_private == NULL) {
        return NULL;
    }
    vol = (mfs_vfs_volume_t *)mnt->fs_private;
    if (!vol->armed || !vol->fs.mounted) {
        return NULL;
    }
    return &vol->fs;
}

static int mfs_vfs_is_slash(char c)
{
    return c == '/';
}

static void mfs_vfs_str_copy(char *out, metta_u32 cap, const char *src)
{
    metta_u32 i = 0;

    if (out == NULL || cap == 0u) {
        return;
    }
    if (src == NULL) {
        out[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1u < cap) {
        out[i] = src[i];
        i++;
    }
    out[i] = '\0';
}

static vfs_node_t *mfs_vfs_node_alloc(vfs_mount_t *mnt, uint32_t ino, vfs_node_type_t type)
{
    for (metta_u32 i = 0; i < MFS_VFS_NODE_POOL; i++) {
        if (g_node_used[i] == 0u) {
            g_node_used[i] = 1u;
            g_nodes[i].type = type;
            g_nodes[i].fs_cookie = (void *)(uintptr_t)ino;
            g_nodes[i].mount = mnt;
            g_nodes[i].size = 0;
            return &g_nodes[i];
        }
    }
    return NULL;
}

static void mfs_vfs_node_free(vfs_node_t *node)
{
    metta_u32 i;

    if (node == NULL) {
        return;
    }
    for (i = 0; i < MFS_VFS_NODE_POOL; i++) {
        if (&g_nodes[i] == node) {
            g_node_used[i] = 0u;
            node->fs_cookie = NULL;
            node->mount = NULL;
            return;
        }
    }
}

static mfs_vfs_handle_t *mfs_vfs_handle_alloc(uint32_t ino, int is_dir)
{
    for (metta_u32 i = 0; i < MFS_VFS_HANDLE_POOL; i++) {
        if (g_handles[i].used == 0u) {
            g_handles[i].used = 1u;
            g_handles[i].is_dir = (uint8_t)(is_dir ? 1 : 0);
            g_handles[i].ino = ino;
            if (is_dir) {
                (void)mfs_readdir_init(&g_handles[i].dir_ctx);
            }
            return &g_handles[i];
        }
    }
    return NULL;
}

static void mfs_vfs_handle_free(mfs_vfs_handle_t *h)
{
    if (h == NULL) {
        return;
    }
    h->used = 0u;
    h->is_dir = 0u;
    h->ino = 0u;
}

static mfs_vfs_handle_t *mfs_vfs_handle_from_ptr(void *handle)
{
    return (mfs_vfs_handle_t *)handle;
}

static int mfs_vfs_resolve_ino(mfs_fs_t *fs, const char *rel, uint32_t *ino_out)
{
    char            walk[VFS_PATH_MAX];
    char            comp[VFS_NAME_MAX];
    const char     *cursor;
    uint32_t        cur;
    uint32_t        found;
    metta_u32       len;

    if (fs == NULL || ino_out == NULL) {
        return MFS_EINVAL;
    }

    cur = mfs_root_ino(fs);
    if (rel == NULL || rel[0] == '\0') {
        *ino_out = cur;
        return MFS_OK;
    }

    mfs_vfs_str_copy(walk, sizeof(walk), rel);
    cursor = walk;

    while (mfs_vfs_is_slash(*cursor)) {
        cursor++;
    }
    if (*cursor == '\0') {
        *ino_out = cur;
        return MFS_OK;
    }

    for (;;) {
        len = 0u;
        while (cursor[len] != '\0' && !mfs_vfs_is_slash(cursor[len])) {
            if (len + 1u >= VFS_NAME_MAX) {
                return MFS_EINVAL;
            }
            comp[len] = cursor[len];
            len++;
        }
        comp[len] = '\0';

        if (len == 0u) {
            break;
        }

        if (mfs_lookup(fs, cur, comp, &found) != MFS_OK) {
            return MFS_ENOENT;
        }
        cur = found;

        cursor += len;
        while (mfs_vfs_is_slash(*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
    }

    *ino_out = cur;
    return MFS_OK;
}

static int mfs_vfs_split_parent(const char *rel, char *parent, char *base)
{
    char tmp[VFS_PATH_MAX];
    metta_u32 len;
    metta_u32 last_slash = 0;
    metta_u32 saw_slash = 0u;

    if (rel == NULL || parent == NULL || base == NULL) {
        return MFS_EINVAL;
    }

    mfs_vfs_str_copy(tmp, sizeof(tmp), rel);
    len = 0u;
    while (tmp[len] != '\0') {
        len++;
    }

    for (metta_u32 i = 0u; i < len; i++) {
        if (mfs_vfs_is_slash(tmp[i])) {
            last_slash = i;
            saw_slash = 1u;
        }
    }

    if (saw_slash == 0u) {
        parent[0] = '\0';
        mfs_vfs_str_copy(base, VFS_NAME_MAX, tmp);
        return MFS_OK;
    }

    tmp[last_slash] = '\0';
    mfs_vfs_str_copy(parent, VFS_PATH_MAX, tmp);
    mfs_vfs_str_copy(base, VFS_NAME_MAX, &rel[last_slash + 1u]);
    return MFS_OK;
}

static vfs_node_type_t mfs_vfs_type_from_inode(const mfs_inode_disk_t *id)
{
    if (id != NULL && mfs_inode_mode_is_dir(id->mode)) {
        return VFS_NODE_DIR;
    }
    return VFS_NODE_FILE;
}

static metta_status_t mfs_vfs_lookup(vfs_mount_t *mnt, const char *rel_path, vfs_node_t **out)
{
    mfs_fs_t         *fs;
    uint32_t          ino;
    mfs_inode_disk_t  id;
    vfs_node_t       *node;
    int               st;

    if (out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    fs = mfs_vfs_fs(mnt);
    if (fs == NULL) {
        return METTA_ERR_UNINITIALIZED;
    }

    st = mfs_vfs_resolve_ino(fs, rel_path, &ino);
    if (st != MFS_OK) {
        return mfs_status_to_metta(st);
    }
    if (mfs_read_inode(fs, ino, &id) != MFS_OK) {
        return METTA_ERR_CORRUPT;
    }

    node = mfs_vfs_node_alloc(mnt, ino, mfs_vfs_type_from_inode(&id));
    if (node == NULL) {
        return METTA_ERR_NO_MEMORY;
    }
    node->size = id.size;

    *out = node;
    return METTA_OK;
}

static metta_status_t mfs_vfs_create(vfs_mount_t *mnt, const char *rel_path, vfs_node_type_t type)
{
    mfs_fs_t   *fs;
    char        parent[VFS_PATH_MAX];
    char        base[VFS_NAME_MAX];
    uint32_t    dir_ino;
    uint32_t    created = 0u;
    int         st;

    fs = mfs_vfs_fs(mnt);
    if (fs == NULL) {
        return METTA_ERR_UNINITIALIZED;
    }
    if (type == VFS_NODE_DIR) {
        return mfs_vfs_mkdir(mnt, rel_path);
    }
    if (type != VFS_NODE_FILE) {
        return METTA_ERR_INVALID_ARG;
    }

    st = mfs_vfs_split_parent(rel_path, parent, base);
    if (st != MFS_OK) {
        return mfs_status_to_metta(st);
    }
    st = mfs_vfs_resolve_ino(fs, parent, &dir_ino);
    if (st != MFS_OK) {
        return mfs_status_to_metta(st);
    }

    st = mfs_create_file(fs, dir_ino, base, &created);
    return mfs_status_to_metta(st);
}

static metta_status_t mfs_vfs_mkdir(vfs_mount_t *mnt, const char *rel_path)
{
    mfs_fs_t *fs;
    char      parent[VFS_PATH_MAX];
    char      base[VFS_NAME_MAX];
    uint32_t  dir_ino;
    int       st;

    fs = mfs_vfs_fs(mnt);
    if (fs == NULL) {
        return METTA_ERR_UNINITIALIZED;
    }

    st = mfs_vfs_split_parent(rel_path, parent, base);
    if (st != MFS_OK) {
        return mfs_status_to_metta(st);
    }
    st = mfs_vfs_resolve_ino(fs, parent, &dir_ino);
    if (st != MFS_OK) {
        return mfs_status_to_metta(st);
    }

    st = mfs_mkdir(fs, dir_ino, base, NULL);
    return mfs_status_to_metta(st);
}

static metta_status_t mfs_vfs_open_node(vfs_mount_t *mnt, vfs_node_t *node, metta_u32 flags,
                                      void **handle_out)
{
    mfs_fs_t          *fs;
    uint32_t           ino;
    mfs_inode_disk_t   id;
    mfs_vfs_handle_t  *h;

    (void)flags;

    if (node == NULL || handle_out == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    fs = mfs_vfs_fs(mnt);
    if (fs == NULL) {
        return METTA_ERR_UNINITIALIZED;
    }

    ino = (uint32_t)(uintptr_t)node->fs_cookie;
    if (mfs_read_inode(fs, ino, &id) != MFS_OK) {
        return METTA_ERR_CORRUPT;
    }

    if ((flags & VFS_O_TRUNC) != 0u && node->type == VFS_NODE_FILE) {
        (void)mfs_inode_truncate_blocks(fs, &id, 0u);
        if (mfs_store_inode(fs, &id) != MFS_OK) {
            return METTA_ERR_INTERNAL;
        }
    }

    h = mfs_vfs_handle_alloc(ino, node->type == VFS_NODE_DIR ? 1 : 0);
    if (h == NULL) {
        return METTA_ERR_NO_MEMORY;
    }

    *handle_out = h;
    return METTA_OK;
}

static void mfs_vfs_close_node(vfs_mount_t *mnt, void *handle)
{
    mfs_vfs_handle_t *h;

    (void)mnt;
    h = mfs_vfs_handle_from_ptr(handle);
    mfs_vfs_handle_free(h);
}

static metta_ssize_t mfs_vfs_read(vfs_mount_t *mnt, void *handle, void *buf, metta_u64 len,
                                  metta_u64 *offset_io)
{
    mfs_fs_t         *fs;
    mfs_vfs_handle_t *h;
    size_t            n = 0;
    int               st;

    if (buf == NULL || offset_io == NULL || len == 0u) {
        return (metta_ssize_t)VFS_EINVAL;
    }

    fs = mfs_vfs_fs(mnt);
    h = mfs_vfs_handle_from_ptr(handle);
    if (fs == NULL || h == NULL || h->used == 0u || h->is_dir != 0u) {
        return (metta_ssize_t)VFS_EIO;
    }

    st = mfs_read_file(fs, h->ino, *offset_io, buf, (size_t)len, &n);
    if (st != MFS_OK) {
        return (metta_ssize_t)vfs_errno_from_status(mfs_status_to_metta(st));
    }

    *offset_io += (metta_u64)n;
    return (metta_ssize_t)n;
}

static metta_ssize_t mfs_vfs_write(vfs_mount_t *mnt, void *handle, const void *buf, metta_u64 len,
                                   metta_u64 *offset_io)
{
    mfs_fs_t         *fs;
    mfs_vfs_handle_t *h;
    size_t            n = 0;
    int               st;

    if (buf == NULL || offset_io == NULL || len == 0u) {
        return (metta_ssize_t)VFS_EINVAL;
    }

    fs = mfs_vfs_fs(mnt);
    h = mfs_vfs_handle_from_ptr(handle);
    if (fs == NULL || h == NULL || h->used == 0u || h->is_dir != 0u) {
        return (metta_ssize_t)VFS_EIO;
    }

    st = mfs_write_file(fs, h->ino, *offset_io, buf, (size_t)len, &n);
    if (st != MFS_OK) {
        return (metta_ssize_t)vfs_errno_from_status(mfs_status_to_metta(st));
    }

    *offset_io += (metta_u64)n;
    return (metta_ssize_t)n;
}

static int mfs_vfs_skip_dot(const char *name)
{
    if (name == NULL) {
        return 1;
    }
    if (name[0] == '.' && name[1] == '\0') {
        return 1;
    }
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        return 1;
    }
    return 0;
}

static metta_status_t mfs_vfs_readdir(vfs_mount_t *mnt, void *dir_handle, vfs_dirent_t *ent,
                                      int *more)
{
    mfs_fs_t            *fs;
    mfs_vfs_handle_t    *h;
    mfs_dirent_info_t    raw;
    mfs_inode_disk_t     id;
    int                  st;

    if (ent == NULL || more == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    fs = mfs_vfs_fs(mnt);
    h = mfs_vfs_handle_from_ptr(dir_handle);
    if (fs == NULL || h == NULL || h->used == 0u || h->is_dir == 0u) {
        return METTA_ERR_INVALID_ARG;
    }

    for (;;) {
        st = mfs_readdir(fs, h->ino, &h->dir_ctx, &raw);
        if (st == MFS_ENOENT) {
            *more = 0;
            return METTA_OK;
        }
        if (st != MFS_OK) {
            return mfs_status_to_metta(st);
        }
        if (mfs_vfs_skip_dot(raw.name)) {
            continue;
        }

        mfs_vfs_str_copy(ent->name, VFS_NAME_MAX, raw.name);
        ent->type = (raw.file_type == MFS_FT_DIR) ? VFS_NODE_DIR : VFS_NODE_FILE;

        if (mfs_read_inode(fs, raw.ino, &id) == MFS_OK) {
            ent->size = id.size;
            ent->mode = id.mode;
        } else {
            ent->size = 0;
            ent->mode = 0;
        }

        *more = 1;
        return METTA_OK;
    }
}

static void mfs_vfs_release_node(vfs_mount_t *mnt, vfs_node_t *node)
{
    (void)mnt;
    mfs_vfs_node_free(node);
}

vfs_fs_ops_t mfs_vfs_ops = {
    .fs_name = "mfs",
    .lookup = mfs_vfs_lookup,
    .create = mfs_vfs_create,
    .mkdir = mfs_vfs_mkdir,
    .open_node = mfs_vfs_open_node,
    .close_node = mfs_vfs_close_node,
    .read = mfs_vfs_read,
    .write = mfs_vfs_write,
    .readdir = mfs_vfs_readdir,
    .release_node = mfs_vfs_release_node,
};

static metta_status_t mfs_vfs_prepare_fs(mfs_vfs_volume_t *vol, metta_block_dev_t *bdev)
{
    mfs_format_opts_t opts;
    int               st;

    if (vol == NULL || bdev == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    if (mfs_blkdev_bind_block(&vol->blk, bdev) != MFS_DEV_OK) {
        return METTA_ERR_INVALID_ARG;
    }

    vol->bdev = bdev;
    vol->fs.dev = &vol->blk;
    vol->fs.mounted = 0;

    st = mfs_mount(&vol->blk, &vol->fs);
    if (st == MFS_ECORRUPT && bdev == block_lookup("vd0")) {
        opts.block_count = vol->blk.block_count;
        opts.inode_count = MFS_DEFAULT_INODE_COUNT;
        opts.inode_table_blocks = MFS_DEFAULT_INODE_TBL_BLOCKS;
        opts.label = bdev->name;
        st = mfs_format(&vol->blk, &opts);
        if (st != MFS_OK) {
            return mfs_status_to_metta(st);
        }
        st = mfs_mount(&vol->blk, &vol->fs);
    }
    if (st != MFS_OK) {
        return mfs_status_to_metta(st);
    }

    vol->armed = 1;
    return METTA_OK;
}

metta_status_t mfs_vfs_mount_volume(metta_block_dev_t *bdev, const char *mount_path)
{
    metta_status_t st;

    if (mount_path == NULL) {
        return METTA_ERR_INVALID_ARG;
    }

    for (metta_u32 i = 0; i < MFS_VFS_NODE_POOL; i++) {
        g_node_used[i] = 0u;
    }
    for (metta_u32 i = 0; i < MFS_VFS_HANDLE_POOL; i++) {
        g_handles[i].used = 0u;
    }

    g_vol.armed = 0;
    st = mfs_vfs_prepare_fs(&g_vol, bdev);
    if (st != METTA_OK) {
        return st;
    }

    return vfs_mount(&mfs_vfs_ops, mount_path, &g_vol);
}

metta_status_t mfs_vfs_mount_vd0(const char *mount_path)
{
    metta_block_dev_t *bdev = block_lookup(MFS_VFS_VD0_NAME);

    if (bdev == NULL) {
        return METTA_ERR_NOT_FOUND;
    }
    return mfs_vfs_mount_volume(bdev, mount_path);
}

metta_status_t mfs_vfs_kmain_stub(void)
{
    return mfs_vfs_mount_vd0("/");
}
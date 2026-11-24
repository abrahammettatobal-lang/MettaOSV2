/* VFS stub — árbol en memoria para desarrollo UI (SA-26). */

#include "files_vfs.h"

#include <stddef.h>
#include <stdint.h>

typedef struct vfs_stub_node {
    const char              *name;
    uint32_t                 flags;
    uint64_t                 size;
    uint64_t                 mtime;
    const struct vfs_stub_node *const *children;
} vfs_stub_node;

static const vfs_stub_node n_readme = {
    "README.txt", 0, 128, 1700000000u, NULL
};
static const vfs_stub_node n_hosts = {
    "hosts", 0, 256, 1700000100u, NULL
};
static const vfs_stub_node n_docs = {
    "Documents", MDE_FILES_ENT_DIR, 0, 0, NULL
};
static const vfs_stub_node n_down = {
    "Downloads", MDE_FILES_ENT_DIR, 0, 0, NULL
};

static const vfs_stub_node *kids_user[] = { &n_docs, &n_down, NULL };
static const vfs_stub_node n_user = {
    "user", MDE_FILES_ENT_DIR, 0, 0, kids_user
};

static const vfs_stub_node *kids_home[] = { &n_user, NULL };
static const vfs_stub_node n_home = {
    "home", MDE_FILES_ENT_DIR, 0, 0, kids_home
};

static const vfs_stub_node *kids_mfs[] = { &n_readme, NULL };
static const vfs_stub_node n_mfs = {
    "mfs", MDE_FILES_ENT_DIR, 0, 0, kids_mfs
};

static const vfs_stub_node *kids_etc[] = { &n_hosts, NULL };
static const vfs_stub_node n_etc = {
    "etc", MDE_FILES_ENT_DIR, 0, 0, kids_etc
};

static const vfs_stub_node *kids_root[] = { &n_home, &n_mfs, &n_etc, NULL };
static const vfs_stub_node n_root = {
    "", MDE_FILES_ENT_DIR, 0, 0, kids_root
};

static const vfs_stub_node *vfs_stub_resolve_node(const char *path)
{
    if (path == NULL) {
        return NULL;
    }
    if (path[0] == '/' && path[1] == '\0') {
        return &n_root;
    }

    const vfs_stub_node *node = &n_root;
    const char *p = path;

    if (*p == '/') {
        p++;
    }

    while (*p != '\0') {
        const char *slash = p;
        while (*slash != '\0' && *slash != '/') {
            slash++;
        }

        size_t seg_len = (size_t)(slash - p);
        if (seg_len == 0) {
            return NULL;
        }

        const vfs_stub_node *next = NULL;
        if (node->children != NULL) {
            for (const vfs_stub_node *const *c = node->children; *c != NULL; c++) {
                const char *name = (*c)->name;
                size_t nlen = 0;
                while (name[nlen] != '\0') {
                    nlen++;
                }
                if (nlen == seg_len) {
                    size_t i = 0;
                    for (; i < seg_len; i++) {
                        if (p[i] != name[i]) {
                            break;
                        }
                    }
                    if (i == seg_len) {
                        next = *c;
                        break;
                    }
                }
            }
        }

        if (next == NULL) {
            return NULL;
        }
        node = next;
        p = slash;
        if (*p == '/') {
            p++;
        }
    }

    return node;
}

static void vfs_stub_fill_dirent(const vfs_stub_node *node, mde_files_dirent_t *out)
{
    size_t i = 0;
    const char *src = node->name;

    while (src[i] != '\0' && i < MDE_FILES_NAME_MAX) {
        out->name[i] = src[i];
        i++;
    }
    if (i == 0) {
        out->name[0] = '/';
        out->name[1] = '\0';
    } else {
        out->name[i] = '\0';
    }

    out->flags      = node->flags;
    out->size_bytes = node->size;
    out->mtime_unix = node->mtime;
}

static mde_files_vfs_status vfs_stub_resolve(void *ctx, const char *path, mde_files_dirent_t *out)
{
    (void)ctx;
    if (out == NULL || path == NULL) {
        return MDE_FILES_VFS_INVALID;
    }

    const vfs_stub_node *node = vfs_stub_resolve_node(path);
    if (node == NULL) {
        return MDE_FILES_VFS_NOT_FOUND;
    }

    vfs_stub_fill_dirent(node, out);
    return MDE_FILES_VFS_OK;
}

static mde_files_vfs_status vfs_stub_read_dir(
    void *ctx,
    const char *path,
    mde_files_dirent_t *entries,
    size_t cap,
    size_t *out_count)
{
    (void)ctx;
    if (entries == NULL || out_count == NULL || path == NULL) {
        return MDE_FILES_VFS_INVALID;
    }

    const vfs_stub_node *node = vfs_stub_resolve_node(path);
    if (node == NULL) {
        return MDE_FILES_VFS_NOT_FOUND;
    }
    if ((node->flags & MDE_FILES_ENT_DIR) == 0) {
        return MDE_FILES_VFS_NOT_DIR;
    }

    size_t n = 0;
    if (node->children != NULL && cap > 0) {
        for (const vfs_stub_node *const *c = node->children; *c != NULL && n < cap; c++) {
            vfs_stub_fill_dirent(*c, &entries[n]);
            n++;
        }
    }

    *out_count = n;
    return MDE_FILES_VFS_OK;
}

static mde_files_vfs_status vfs_stub_parent_path(void *ctx, const char *path, char *parent, size_t parent_cap)
{
    (void)ctx;
    if (path == NULL || parent == NULL || parent_cap == 0) {
        return MDE_FILES_VFS_INVALID;
    }

    if (path[0] != '/' || (path[0] == '/' && path[1] == '\0')) {
        parent[0] = '/';
        parent[1] = '\0';
        return MDE_FILES_VFS_OK;
    }

    size_t len = 0;
    while (path[len] != '\0' && len < parent_cap) {
        parent[len] = path[len];
        len++;
    }
    if (len >= parent_cap) {
        return MDE_FILES_VFS_INVALID;
    }

    while (len > 1 && parent[len - 1] != '/') {
        len--;
    }
    if (len > 1) {
        parent[len - 1] = '\0';
    } else {
        parent[0] = '/';
        parent[1] = '\0';
    }

    mde_files_path_normalize(parent, parent_cap);
    return MDE_FILES_VFS_OK;
}

static const mde_files_vfs_ops g_vfs_stub_ops = {
    .ctx         = NULL,
    .resolve     = vfs_stub_resolve,
    .read_dir    = vfs_stub_read_dir,
    .parent_path = vfs_stub_parent_path,
};

mde_files_vfs_status mde_files_vfs_bind_stub(mde_files_vfs *vfs)
{
    if (vfs == NULL) {
        return MDE_FILES_VFS_INVALID;
    }
    vfs->ops          = &g_vfs_stub_ops;
    vfs->initialized  = 1;
    return MDE_FILES_VFS_OK;
}

mde_files_vfs_status mde_files_vfs_bind(mde_files_vfs *vfs, const mde_files_vfs_ops *ops)
{
    if (vfs == NULL || ops == NULL) {
        return MDE_FILES_VFS_INVALID;
    }
    if (ops->resolve == NULL || ops->read_dir == NULL || ops->parent_path == NULL) {
        return MDE_FILES_VFS_INVALID;
    }
    vfs->ops         = ops;
    vfs->initialized = 1;
    return MDE_FILES_VFS_OK;
}

void mde_files_path_normalize(char *path, size_t cap)
{
    if (path == NULL || cap == 0) {
        return;
    }

    size_t w = 0;
    for (size_t r = 0; path[r] != '\0' && w + 1 < cap; r++) {
        char c = path[r];
        if (c == '\\') {
            c = '/';
        }
        path[w++] = c;
    }
    path[w] = '\0';

    if (path[0] != '/') {
        if (cap < 2) {
            return;
        }
        size_t len = w;
        for (size_t i = len; i > 0; i--) {
            path[i] = path[i - 1];
        }
        path[0] = '/';
        w++;
        path[w] = '\0';
    }

    if (w > 1 && path[w - 1] == '/') {
        path[w - 1] = '\0';
    }
    if (path[0] == '\0') {
        path[0] = '/';
        path[1] = '\0';
    }
}

int mde_files_path_is_child(const char *parent, const char *child)
{
    if (parent == NULL || child == NULL) {
        return 0;
    }

    size_t plen = 0;
    while (parent[plen] != '\0') {
        plen++;
    }
    size_t clen = 0;
    while (child[clen] != '\0') {
        clen++;
    }

    if (clen <= plen) {
        return 0;
    }
    if (child[plen] != '/' && !(plen == 1 && parent[0] == '/')) {
        return 0;
    }

    size_t i = 0;
    while (i < plen && parent[i] == child[i]) {
        i++;
    }
    return i == plen;
}
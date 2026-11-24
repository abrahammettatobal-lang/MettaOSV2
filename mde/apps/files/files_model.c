#include "files_model.h"

#include <stddef.h>
#include <stdint.h>

static void path_copy(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void history_push(
    mde_files_history_entry *stack,
    size_t *count,
    size_t max,
    const char *path)
{
    if (*count >= max) {
        for (size_t i = 1; i < max; i++) {
            path_copy(stack[i - 1].path, MDE_FILES_PATH_MAX, stack[i].path);
        }
        *count = max - 1;
    }
    path_copy(stack[*count].path, MDE_FILES_PATH_MAX, path);
    (*count)++;
}

static int history_pop(mde_files_history_entry *stack, size_t *count, char *out, size_t cap)
{
    if (*count == 0) {
        return 0;
    }
    (*count)--;
    path_copy(out, cap, stack[*count].path);
    stack[*count].path[0] = '\0';
    return 1;
}

static void history_clear(mde_files_history_entry *stack, size_t *count)
{
    for (size_t i = 0; i < *count; i++) {
        stack[i].path[0] = '\0';
    }
    *count = 0;
}

static mde_files_vfs_status join_child_path(
    const char *parent,
    const char *name,
    char *out,
    size_t cap)
{
    if (parent == NULL || name == NULL || out == NULL) {
        return MDE_FILES_VFS_INVALID;
    }

    size_t plen = 0;
    while (parent[plen] != '\0') {
        plen++;
    }
    size_t nlen = 0;
    while (name[nlen] != '\0') {
        nlen++;
    }

    size_t need_slash = (plen > 1 || parent[0] != '/') ? 1 : 0;
    if (parent[0] == '/' && parent[1] == '\0') {
        need_slash = 0;
    }

    if (plen + need_slash + nlen + 1 > cap) {
        return MDE_FILES_VFS_INVALID;
    }

    size_t w = 0;
    for (size_t i = 0; i < plen; i++) {
        out[w++] = parent[i];
    }
    if (need_slash) {
        out[w++] = '/';
    }
    for (size_t i = 0; i < nlen; i++) {
        out[w++] = name[i];
    }
    out[w] = '\0';
    mde_files_path_normalize(out, cap);
    return MDE_FILES_VFS_OK;
}

void mde_files_model_init(mde_files_model *model)
{
    if (model == NULL) {
        return;
    }

    model->vfs.ops         = NULL;
    model->vfs.initialized = 0;
    model->current_path[0] = '/';
    model->current_path[1] = '\0';
    model->entry_count     = 0;
    model->selected_index  = -1;
    model->scroll_row      = 0;
    model->back_count      = 0;
    model->forward_count   = 0;
    model->last_error      = MDE_FILES_VFS_OK;
}

mde_files_vfs_status mde_files_model_mount_stub(mde_files_model *model)
{
    if (model == NULL) {
        return MDE_FILES_VFS_INVALID;
    }
    return mde_files_vfs_bind_stub(&model->vfs);
}

mde_files_vfs_status mde_files_model_refresh(mde_files_model *model)
{
    if (model == NULL || model->vfs.ops == NULL) {
        return MDE_FILES_VFS_INVALID;
    }

    size_t count = 0;
    mde_files_vfs_status st = model->vfs.ops->read_dir(
        model->vfs.ops->ctx,
        model->current_path,
        model->entries,
        MDE_FILES_LIST_CAP,
        &count);

    model->entry_count = count;
    model->last_error  = st;

    if (model->selected_index >= (int32_t)count) {
        model->selected_index = count > 0 ? 0 : -1;
    }
    if (count == 0) {
        model->selected_index = -1;
    }

    return st;
}

static mde_files_vfs_status navigate_internal(mde_files_model *model, const char *path, int push_history)
{
    if (model == NULL || path == NULL || model->vfs.ops == NULL) {
        return MDE_FILES_VFS_INVALID;
    }

    char normalized[MDE_FILES_PATH_MAX];
    path_copy(normalized, sizeof normalized, path);
    mde_files_path_normalize(normalized, sizeof normalized);

    mde_files_dirent_t probe;
    mde_files_vfs_status st = model->vfs.ops->resolve(model->vfs.ops->ctx, normalized, &probe);
    if (st != MDE_FILES_VFS_OK) {
        model->last_error = st;
        return st;
    }
    if ((probe.flags & MDE_FILES_ENT_DIR) == 0) {
        model->last_error = MDE_FILES_VFS_NOT_DIR;
        return MDE_FILES_VFS_NOT_DIR;
    }

    if (push_history) {
        history_push(model->back_stack, &model->back_count, MDE_FILES_HISTORY_DEPTH, model->current_path);
        history_clear(model->forward_stack, &model->forward_count);
    }

    path_copy(model->current_path, MDE_FILES_PATH_MAX, normalized);
    model->selected_index = -1;
    model->scroll_row     = 0;

    return mde_files_model_refresh(model);
}

mde_files_vfs_status mde_files_model_navigate(mde_files_model *model, const char *path)
{
    return navigate_internal(model, path, 1);
}

mde_files_vfs_status mde_files_model_go_up(mde_files_model *model)
{
    if (model == NULL || model->vfs.ops == NULL) {
        return MDE_FILES_VFS_INVALID;
    }

    char parent[MDE_FILES_PATH_MAX];
    mde_files_vfs_status st = model->vfs.ops->parent_path(
        model->vfs.ops->ctx,
        model->current_path,
        parent,
        sizeof parent);
    if (st != MDE_FILES_VFS_OK) {
        model->last_error = st;
        return st;
    }

    return navigate_internal(model, parent, 1);
}

mde_files_vfs_status mde_files_model_open_selected(mde_files_model *model)
{
    if (model == NULL || model->selected_index < 0) {
        return MDE_FILES_VFS_INVALID;
    }

    const mde_files_dirent_t *ent = mde_files_model_entry(model, (size_t)model->selected_index);
    if (ent == NULL) {
        return MDE_FILES_VFS_NOT_FOUND;
    }

    if ((ent->flags & MDE_FILES_ENT_DIR) == 0) {
        /* Apertura de archivos: lanzador / visor (misión futura). */
        model->last_error = MDE_FILES_VFS_OK;
        return MDE_FILES_VFS_OK;
    }

    char child[MDE_FILES_PATH_MAX];
    mde_files_vfs_status st = join_child_path(model->current_path, ent->name, child, sizeof child);
    if (st != MDE_FILES_VFS_OK) {
        model->last_error = st;
        return st;
    }
    return navigate_internal(model, child, 1);
}

mde_files_vfs_status mde_files_model_back(mde_files_model *model)
{
    if (model == NULL) {
        return MDE_FILES_VFS_INVALID;
    }

    char prior[MDE_FILES_PATH_MAX];
    if (!history_pop(model->back_stack, &model->back_count, prior, sizeof prior)) {
        return MDE_FILES_VFS_OK;
    }

    history_push(model->forward_stack, &model->forward_count, MDE_FILES_HISTORY_DEPTH, model->current_path);
    return navigate_internal(model, prior, 0);
}

mde_files_vfs_status mde_files_model_forward(mde_files_model *model)
{
    if (model == NULL) {
        return MDE_FILES_VFS_INVALID;
    }

    char next[MDE_FILES_PATH_MAX];
    if (!history_pop(model->forward_stack, &model->forward_count, next, sizeof next)) {
        return MDE_FILES_VFS_OK;
    }

    history_push(model->back_stack, &model->back_count, MDE_FILES_HISTORY_DEPTH, model->current_path);
    return navigate_internal(model, next, 0);
}

void mde_files_model_select(mde_files_model *model, int32_t index)
{
    if (model == NULL) {
        return;
    }
    if (index < 0 || (size_t)index >= model->entry_count) {
        model->selected_index = -1;
        return;
    }
    model->selected_index = index;
}

void mde_files_model_select_delta(mde_files_model *model, int32_t delta)
{
    if (model == NULL || model->entry_count == 0) {
        return;
    }

    int32_t next = model->selected_index < 0 ? 0 : model->selected_index + delta;
    if (next < 0) {
        next = 0;
    }
    if ((size_t)next >= model->entry_count) {
        next = (int32_t)model->entry_count - 1;
    }
    model->selected_index = next;

    if (next < model->scroll_row) {
        model->scroll_row = next;
    }
    if (next >= model->scroll_row + 16) {
        model->scroll_row = next - 15;
    }
}

int mde_files_model_can_back(const mde_files_model *model)
{
    return model != NULL && model->back_count > 0;
}

int mde_files_model_can_forward(const mde_files_model *model)
{
    return model != NULL && model->forward_count > 0;
}

const mde_files_dirent_t *mde_files_model_entry(const mde_files_model *model, size_t index)
{
    if (model == NULL || index >= model->entry_count) {
        return NULL;
    }
    return &model->entries[index];
}
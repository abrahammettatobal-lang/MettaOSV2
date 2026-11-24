#ifndef MDE_FILES_MODEL_H
#define MDE_FILES_MODEL_H

#include "files_vfs.h"

#include <stddef.h>
#include <stdint.h>

#define MDE_FILES_LIST_CAP      64
#define MDE_FILES_HISTORY_DEPTH 24

typedef struct {
    char path[MDE_FILES_PATH_MAX];
} mde_files_history_entry;

typedef struct {
    mde_files_vfs vfs;

    char current_path[MDE_FILES_PATH_MAX];
    mde_files_dirent_t entries[MDE_FILES_LIST_CAP];
    size_t             entry_count;

    int32_t selected_index;
    int32_t scroll_row;

    mde_files_history_entry back_stack[MDE_FILES_HISTORY_DEPTH];
    size_t                  back_count;
    mde_files_history_entry forward_stack[MDE_FILES_HISTORY_DEPTH];
    size_t                  forward_count;

    mde_files_vfs_status last_error;
} mde_files_model;

void mde_files_model_init(mde_files_model *model);
mde_files_vfs_status mde_files_model_mount_stub(mde_files_model *model);

mde_files_vfs_status mde_files_model_refresh(mde_files_model *model);

mde_files_vfs_status mde_files_model_navigate(mde_files_model *model, const char *path);
mde_files_vfs_status mde_files_model_go_up(mde_files_model *model);
mde_files_vfs_status mde_files_model_open_selected(mde_files_model *model);

mde_files_vfs_status mde_files_model_back(mde_files_model *model);
mde_files_vfs_status mde_files_model_forward(mde_files_model *model);

void mde_files_model_select(mde_files_model *model, int32_t index);
void mde_files_model_select_delta(mde_files_model *model, int32_t delta);

int mde_files_model_can_back(const mde_files_model *model);
int mde_files_model_can_forward(const mde_files_model *model);

const mde_files_dirent_t *mde_files_model_entry(const mde_files_model *model, size_t index);

#endif /* MDE_FILES_MODEL_H */
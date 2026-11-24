#include "files.h"

#include <stddef.h>

int mde_files_app_create(mde_files_app *app)
{
    if (app == NULL) {
        return -1;
    }

    app->client_w  = MDE_FILES_APP_DEFAULT_W;
    app->client_h  = MDE_FILES_APP_DEFAULT_H;
    app->mounted   = 0;

    mde_files_model_init(&app->model);

    mde_files_vfs_status st = mde_files_model_mount_stub(&app->model);
    if (st != MDE_FILES_VFS_OK) {
        return -2;
    }

    st = mde_files_model_navigate(&app->model, "/home/user");
    if (st != MDE_FILES_VFS_OK) {
        st = mde_files_model_navigate(&app->model, "/");
        if (st != MDE_FILES_VFS_OK) {
            return -3;
        }
    }

    mde_files_view_layout_compute(&app->layout, app->client_w, app->client_h);
    app->mounted = 1;
    return 0;
}

void mde_files_app_destroy(mde_files_app *app)
{
    if (app == NULL) {
        return;
    }
    app->mounted = 0;
    app->model.vfs.initialized = 0;
    app->model.vfs.ops         = NULL;
}

void mde_files_app_resize(mde_files_app *app, int32_t w, int32_t h)
{
    if (app == NULL) {
        return;
    }
    app->client_w = w;
    app->client_h = h;
    mde_files_view_layout_compute(&app->layout, w, h);
}

mde_files_vfs_status mde_files_app_dispatch(mde_files_app *app, const mde_files_action *action)
{
    if (app == NULL || action == NULL || !app->mounted) {
        return MDE_FILES_VFS_INVALID;
    }

    mde_files_model *m = &app->model;

    switch (action->kind) {
    case MDE_FILES_ACTION_BACK:
        return mde_files_model_back(m);
    case MDE_FILES_ACTION_FORWARD:
        return mde_files_model_forward(m);
    case MDE_FILES_ACTION_UP:
        return mde_files_model_go_up(m);
    case MDE_FILES_ACTION_OPEN:
        return mde_files_model_open_selected(m);
    case MDE_FILES_ACTION_REFRESH:
        return mde_files_model_refresh(m);
    case MDE_FILES_ACTION_SELECT_ROW:
        mde_files_model_select(m, action->row_index);
        return MDE_FILES_VFS_OK;
    case MDE_FILES_ACTION_NAVIGATE:
        return mde_files_model_navigate(m, action->path);
    case MDE_FILES_ACTION_NONE:
    default:
        return MDE_FILES_VFS_OK;
    }
}

void mde_files_app_paint(mde_files_app *app, const mde_files_painter *painter)
{
    if (app == NULL || !app->mounted) {
        return;
    }
    mde_files_view_paint(&app->layout, &app->model, painter);
}

const char *mde_files_app_id(void)
{
    return MDE_FILES_APP_ID;
}

const char *mde_files_app_title(void)
{
    return MDE_FILES_APP_TITLE;
}
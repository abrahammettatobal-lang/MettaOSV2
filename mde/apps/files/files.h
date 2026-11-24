#ifndef MDE_FILES_APP_H
#define MDE_FILES_APP_H

#include "files_model.h"
#include "files_view.h"

#include <stdint.h>

#define MDE_FILES_APP_ID          "metta.files"
#define MDE_FILES_APP_TITLE       "Archivos"
#define MDE_FILES_APP_DEFAULT_W   720
#define MDE_FILES_APP_DEFAULT_H   480

typedef enum {
    MDE_FILES_ACTION_NONE = 0,
    MDE_FILES_ACTION_BACK,
    MDE_FILES_ACTION_FORWARD,
    MDE_FILES_ACTION_UP,
    MDE_FILES_ACTION_OPEN,
    MDE_FILES_ACTION_REFRESH,
    MDE_FILES_ACTION_SELECT_ROW,
    MDE_FILES_ACTION_NAVIGATE,
} mde_files_action_kind;

typedef struct {
    mde_files_action_kind kind;
    int32_t               row_index;
    char                  path[MDE_FILES_PATH_MAX];
} mde_files_action;

typedef struct {
    mde_files_model       model;
    mde_files_view_layout layout;
    int32_t               client_w;
    int32_t               client_h;
    int                   mounted;
} mde_files_app;

int  mde_files_app_create(mde_files_app *app);
void mde_files_app_destroy(mde_files_app *app);

void mde_files_app_resize(mde_files_app *app, int32_t w, int32_t h);

mde_files_vfs_status mde_files_app_dispatch(mde_files_app *app, const mde_files_action *action);

void mde_files_app_paint(mde_files_app *app, const mde_files_painter *painter);

const char *mde_files_app_id(void);
const char *mde_files_app_title(void);

#endif /* MDE_FILES_APP_H */
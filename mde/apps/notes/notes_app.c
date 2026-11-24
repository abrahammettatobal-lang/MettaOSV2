/* MDE Notes — ciclo de vida y comandos de documento (SA-27) */

#include "notes.h"

#include "editor.h"
#include "undo.h"
#include "vfs_io.h"

#include <stddef.h>

static notes_editor_t      g_editor;
static notes_undo_stack_t  g_undo;

static void copy_path(char *dst, const char *src)
{
    size_t i = 0;
    if (dst == NULL || src == NULL) {
        return;
    }
    while (src[i] != '\0' && i + 1u < MDE_NOTES_MAX_PATH) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

notes_status_t notes_app_init(notes_app_t *app)
{
    notes_status_t st;

    if (app == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }

    st = notes_editor_init(&g_editor);
    if (st != NOTES_OK) {
        return st;
    }

    st = notes_undo_init(&g_undo);
    if (st != NOTES_OK) {
        return st;
    }

    app->editor   = &g_editor;
    app->undo     = &g_undo;
    app->path_set = false;
    app->dirty    = false;
    app->path[0]  = '\0';

    (void)notes_undo_checkpoint(&g_undo, &g_editor);
    return NOTES_OK;
}

void notes_app_shutdown(notes_app_t *app)
{
    if (app == NULL) {
        return;
    }
    notes_undo_reset(app->undo);
    notes_editor_reset(app->editor);
    app->path_set = false;
    app->dirty    = false;
    app->path[0]  = '\0';
}

notes_status_t notes_app_new_document(notes_app_t *app)
{
    if (app == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }

    notes_editor_reset(app->editor);
    notes_undo_reset(app->undo);
    (void)notes_undo_checkpoint(app->undo, app->editor);

    app->path_set = false;
    app->dirty    = false;
    app->path[0]  = '\0';
    return NOTES_OK;
}

notes_status_t notes_app_open_path(notes_app_t *app, const char *path)
{
    char load_buf[NOTES_EDITOR_MAX_BYTES];
    size_t nbytes = 0;
    notes_status_t st;

    if (app == NULL || path == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }

    st = notes_vfs_read_file(path, load_buf, sizeof(load_buf), &nbytes);
    if (st != NOTES_OK) {
        return st;
    }

    st = notes_editor_load_text(app->editor, load_buf, nbytes);
    if (st != NOTES_OK) {
        return st;
    }

    notes_undo_reset(app->undo);
    (void)notes_undo_checkpoint(app->undo, app->editor);

    copy_path(app->path, path);
    app->path_set = true;
    app->dirty    = false;
    return NOTES_OK;
}

notes_status_t notes_app_save(notes_app_t *app)
{
    if (app == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (!app->path_set) {
        return NOTES_ERR_INVALID_ARG;
    }
    return notes_app_save_as(app, app->path);
}

notes_status_t notes_app_save_as(notes_app_t *app, const char *path)
{
    char out_buf[NOTES_EDITOR_MAX_BYTES];
    size_t nbytes = 0;
    notes_status_t st;

    if (app == NULL || path == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }

    st = notes_editor_copy_bytes(app->editor, out_buf, sizeof(out_buf), &nbytes);
    if (st != NOTES_OK) {
        return st;
    }

    st = notes_vfs_write_file(path, out_buf, nbytes);
    if (st != NOTES_OK) {
        return st;
    }

    copy_path(app->path, path);
    app->path_set = true;
    app->dirty    = false;
    (void)notes_undo_checkpoint(app->undo, app->editor);
    return NOTES_OK;
}

bool notes_app_is_dirty(const notes_app_t *app)
{
    return (app != NULL) ? app->dirty : false;
}

const char *notes_app_path(const notes_app_t *app)
{
    if (app == NULL || !app->path_set) {
        return NULL;
    }
    return app->path;
}

struct notes_editor *notes_app_editor(notes_app_t *app)
{
    return (app != NULL) ? app->editor : NULL;
}

static notes_status_t edit_with_undo(notes_app_t *app, notes_status_t edit_st)
{
    if (app == NULL || app->editor == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (edit_st != NOTES_OK) {
        return edit_st;
    }
    app->dirty = true;
    return NOTES_OK;
}

notes_status_t notes_app_insert_codepoint(notes_app_t *app, uint32_t ch)
{
    notes_status_t st;

    if (app == NULL || app->editor == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    st = notes_undo_checkpoint(app->undo, app->editor);
    if (st != NOTES_OK) {
        return st;
    }
    return edit_with_undo(app, notes_editor_insert_codepoint(app->editor, ch));
}

notes_status_t notes_app_insert_text(notes_app_t *app, const char *text, size_t len)
{
    notes_status_t st;

    if (app == NULL || app->editor == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    st = notes_undo_checkpoint(app->undo, app->editor);
    if (st != NOTES_OK) {
        return st;
    }
    return edit_with_undo(app, notes_editor_insert_text(app->editor, text, len));
}

notes_status_t notes_app_backspace(notes_app_t *app)
{
    notes_status_t st;

    if (app == NULL || app->editor == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    st = notes_undo_checkpoint(app->undo, app->editor);
    if (st != NOTES_OK) {
        return st;
    }
    return edit_with_undo(app, notes_editor_backspace(app->editor));
}

notes_status_t notes_app_delete_forward(notes_app_t *app)
{
    notes_status_t st;

    if (app == NULL || app->editor == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    st = notes_undo_checkpoint(app->undo, app->editor);
    if (st != NOTES_OK) {
        return st;
    }
    return edit_with_undo(app, notes_editor_delete_forward(app->editor));
}

notes_status_t notes_app_undo(notes_app_t *app)
{
    if (app == NULL || app->editor == NULL || app->undo == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    return notes_undo_apply(app->undo, app->editor);
}

notes_status_t notes_app_redo(notes_app_t *app)
{
    if (app == NULL || app->editor == NULL || app->undo == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    return notes_redo_apply(app->undo, app->editor);
}

bool notes_app_can_undo(const notes_app_t *app)
{
    return (app != NULL && app->undo != NULL) ? notes_undo_can_undo(app->undo) : false;
}

bool notes_app_can_redo(const notes_app_t *app)
{
    return (app != NULL && app->undo != NULL) ? notes_undo_can_redo(app->undo) : false;
}
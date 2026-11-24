#ifndef MDE_NOTES_H
#define MDE_NOTES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MDE_NOTES_APP_ID           "com.mettaos.notes"
#define MDE_NOTES_APP_TITLE        "Notas"
#define MDE_NOTES_MAX_PATH         256u

typedef enum {
    NOTES_OK = 0,
    NOTES_ERR_INVALID_ARG,
    NOTES_ERR_NO_MEMORY,
    NOTES_ERR_NOT_FOUND,
    NOTES_ERR_BUSY,
    NOTES_ERR_PERM,
    NOTES_ERR_UNINITIALIZED,
    NOTES_ERR_CORRUPT,
    NOTES_ERR_RANGE,
    NOTES_ERR_VFS_UNBOUND,
    NOTES_ERR_IO,
    NOTES_ERR_TOO_LARGE
} notes_status_t;

struct notes_editor;
struct notes_undo_stack;

typedef struct notes_app {
    struct notes_editor    *editor;
    struct notes_undo_stack *undo;
    char                    path[MDE_NOTES_MAX_PATH];
    bool                    path_set;
    bool                    dirty;
} notes_app_t;

notes_status_t notes_app_init(notes_app_t *app);
void           notes_app_shutdown(notes_app_t *app);

notes_status_t notes_app_new_document(notes_app_t *app);
notes_status_t notes_app_open_path(notes_app_t *app, const char *path);
notes_status_t notes_app_save(notes_app_t *app);
notes_status_t notes_app_save_as(notes_app_t *app, const char *path);

bool notes_app_is_dirty(const notes_app_t *app);
const char *notes_app_path(const notes_app_t *app);

struct notes_editor *notes_app_editor(notes_app_t *app);

notes_status_t notes_app_insert_codepoint(notes_app_t *app, uint32_t ch);
notes_status_t notes_app_insert_text(notes_app_t *app, const char *text, size_t len);
notes_status_t notes_app_backspace(notes_app_t *app);
notes_status_t notes_app_delete_forward(notes_app_t *app);

notes_status_t notes_app_undo(notes_app_t *app);
notes_status_t notes_app_redo(notes_app_t *app);
bool           notes_app_can_undo(const notes_app_t *app);
bool           notes_app_can_redo(const notes_app_t *app);

#endif /* MDE_NOTES_H */
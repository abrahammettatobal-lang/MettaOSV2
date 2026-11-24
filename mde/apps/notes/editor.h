#ifndef MDE_NOTES_EDITOR_H
#define MDE_NOTES_EDITOR_H

#include "notes.h"

#include <stdint.h>

/* Capacidad del documento en memoria (stub; SA-27 no persiste aún en PMM de usuario). */
#define NOTES_EDITOR_MAX_BYTES   (64u * 1024u)
#define NOTES_EDITOR_MAX_LINES   4096u

typedef struct notes_cursor {
    uint32_t line;
    uint32_t column; /* columna en UTF-8/Latin-1 por código de unidad */
} notes_cursor_t;

typedef struct notes_selection {
    bool           active;
    notes_cursor_t anchor;
    notes_cursor_t caret;
} notes_selection_t;

typedef struct notes_editor {
    char               bytes[NOTES_EDITOR_MAX_BYTES];
    size_t             length;
    uint32_t           line_starts[NOTES_EDITOR_MAX_LINES];
    uint32_t           line_count;
    notes_cursor_t     cursor;
    notes_selection_t  selection;
} notes_editor_t;

notes_status_t notes_editor_init(notes_editor_t *ed);
void           notes_editor_reset(notes_editor_t *ed);

size_t         notes_editor_length(const notes_editor_t *ed);
uint32_t       notes_editor_line_count(const notes_editor_t *ed);
notes_status_t notes_editor_get_line(const notes_editor_t *ed, uint32_t line,
                                     const char **out_start, size_t *out_len);

const notes_cursor_t *notes_editor_cursor(const notes_editor_t *ed);
notes_status_t        notes_editor_set_cursor(notes_editor_t *ed, uint32_t line,
                                              uint32_t column);

notes_status_t notes_editor_insert_codepoint(notes_editor_t *ed, uint32_t ch);
notes_status_t notes_editor_insert_text(notes_editor_t *ed, const char *text, size_t len);
notes_status_t notes_editor_newline(notes_editor_t *ed);
notes_status_t notes_editor_backspace(notes_editor_t *ed);
notes_status_t notes_editor_delete_forward(notes_editor_t *ed);

notes_status_t notes_editor_load_text(notes_editor_t *ed, const char *text, size_t len);
notes_status_t notes_editor_copy_bytes(const notes_editor_t *ed, char *dst, size_t dst_cap,
                                       size_t *out_len);

const notes_selection_t *notes_editor_selection(const notes_editor_t *ed);
notes_status_t           notes_editor_clear_selection(notes_editor_t *ed);
notes_status_t           notes_editor_select_all(notes_editor_t *ed);

#endif /* MDE_NOTES_EDITOR_H */
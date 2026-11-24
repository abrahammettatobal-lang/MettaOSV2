/* MDE Notes — editor de texto multilínea (SA-27) */

#include "editor.h"

#include <stddef.h>

static void notes_editor_reindex_lines(notes_editor_t *ed)
{
    uint32_t line = 0;

    ed->line_starts[0] = 0;
    ed->line_count     = 1;

    for (size_t i = 0; i < ed->length && line + 1 < NOTES_EDITOR_MAX_LINES; i++) {
        if (ed->bytes[i] == '\n') {
            line++;
            ed->line_starts[line] = (uint32_t)(i + 1u);
            ed->line_count       = line + 1u;
        }
    }
}

static uint32_t line_end_offset(const notes_editor_t *ed, uint32_t line)
{
    if (line + 1u < ed->line_count) {
        uint32_t next = ed->line_starts[line + 1u];
        return (next > 0u) ? (next - 1u) : 0u;
    }
    return (uint32_t)ed->length;
}

static void cursor_clamp(notes_editor_t *ed)
{
    if (ed->line_count == 0u) {
        ed->cursor.line   = 0u;
        ed->cursor.column = 0u;
        return;
    }
    if (ed->cursor.line >= ed->line_count) {
        ed->cursor.line = ed->line_count - 1u;
    }

    uint32_t line_start = ed->line_starts[ed->cursor.line];
    uint32_t line_end   = line_end_offset(ed, ed->cursor.line);
    uint32_t line_len   = line_end - line_start;

    if (ed->cursor.column > line_len) {
        ed->cursor.column = line_len;
    }
}

notes_status_t notes_editor_init(notes_editor_t *ed)
{
    if (ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    notes_editor_reset(ed);
    return NOTES_OK;
}

void notes_editor_reset(notes_editor_t *ed)
{
    if (ed == NULL) {
        return;
    }
    ed->length = 0u;
    ed->bytes[0] = '\0';
    ed->cursor.line   = 0u;
    ed->cursor.column = 0u;
    ed->selection.active = false;
    notes_editor_reindex_lines(ed);
}

size_t notes_editor_length(const notes_editor_t *ed)
{
    return (ed != NULL) ? ed->length : 0u;
}

uint32_t notes_editor_line_count(const notes_editor_t *ed)
{
    return (ed != NULL) ? ed->line_count : 0u;
}

notes_status_t notes_editor_get_line(const notes_editor_t *ed, uint32_t line,
                                     const char **out_start, size_t *out_len)
{
    if (ed == NULL || out_start == NULL || out_len == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (line >= ed->line_count) {
        return NOTES_ERR_RANGE;
    }

    uint32_t start = ed->line_starts[line];
    uint32_t end   = line_end_offset(ed, line);
    *out_start     = &ed->bytes[start];
    *out_len       = (size_t)(end - start);
    return NOTES_OK;
}

const notes_cursor_t *notes_editor_cursor(const notes_editor_t *ed)
{
    return (ed != NULL) ? &ed->cursor : NULL;
}

notes_status_t notes_editor_set_cursor(notes_editor_t *ed, uint32_t line, uint32_t column)
{
    if (ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    ed->cursor.line   = line;
    ed->cursor.column = column;
    cursor_clamp(ed);
    return NOTES_OK;
}

static size_t cursor_byte_offset(const notes_editor_t *ed)
{
    return (size_t)ed->line_starts[ed->cursor.line] + (size_t)ed->cursor.column;
}

notes_status_t notes_editor_load_text(notes_editor_t *ed, const char *text, size_t len)
{
    if (ed == NULL || (text == NULL && len != 0u)) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (len >= NOTES_EDITOR_MAX_BYTES) {
        return NOTES_ERR_TOO_LARGE;
    }

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\0') {
            return NOTES_ERR_CORRUPT;
        }
    }

    if (len > 0u) {
        for (size_t i = 0; i < len; i++) {
            ed->bytes[i] = text[i];
        }
    }
    ed->length        = len;
    ed->bytes[len]    = '\0';
    ed->cursor.line   = 0u;
    ed->cursor.column = 0u;
    ed->selection.active = false;
    notes_editor_reindex_lines(ed);
    return NOTES_OK;
}

notes_status_t notes_editor_copy_bytes(const notes_editor_t *ed, char *dst, size_t dst_cap,
                                       size_t *out_len)
{
    if (ed == NULL || dst == NULL || out_len == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (ed->length >= dst_cap) {
        return NOTES_ERR_RANGE;
    }
    for (size_t i = 0; i <= ed->length; i++) {
        dst[i] = ed->bytes[i];
    }
    *out_len = ed->length;
    return NOTES_OK;
}

static notes_status_t insert_at_cursor(notes_editor_t *ed, const char *chunk, size_t chunk_len)
{
    if (chunk_len == 0u) {
        return NOTES_OK;
    }
    if (ed->length + chunk_len >= NOTES_EDITOR_MAX_BYTES) {
        return NOTES_ERR_TOO_LARGE;
    }

    size_t at = cursor_byte_offset(ed);
    if (at > ed->length) {
        return NOTES_ERR_RANGE;
    }

    for (size_t i = ed->length; i > at; i--) {
        ed->bytes[i + chunk_len - 1u] = ed->bytes[i - 1u];
    }
    for (size_t i = 0; i < chunk_len; i++) {
        ed->bytes[at + i] = chunk[i];
    }
    ed->length += chunk_len;
    ed->bytes[ed->length] = '\0';
    notes_editor_reindex_lines(ed);

    for (size_t i = 0; i < chunk_len; i++) {
        if (chunk[i] == '\n') {
            ed->cursor.line++;
            ed->cursor.column = 0u;
        } else {
            ed->cursor.column++;
        }
    }
    cursor_clamp(ed);
    return NOTES_OK;
}

notes_status_t notes_editor_insert_codepoint(notes_editor_t *ed, uint32_t ch)
{
    if (ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (ch > 0xFFu) {
        return NOTES_ERR_INVALID_ARG;
    }
    char c = (char)(ch & 0xFFu);
    return insert_at_cursor(ed, &c, 1u);
}

notes_status_t notes_editor_insert_text(notes_editor_t *ed, const char *text, size_t len)
{
    if (ed == NULL || (text == NULL && len != 0u)) {
        return NOTES_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\0') {
            return NOTES_ERR_CORRUPT;
        }
    }
    return insert_at_cursor(ed, text, len);
}

notes_status_t notes_editor_newline(notes_editor_t *ed)
{
    return notes_editor_insert_codepoint(ed, (uint32_t)'\n');
}

notes_status_t notes_editor_backspace(notes_editor_t *ed)
{
    if (ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (ed->cursor.line == 0u && ed->cursor.column == 0u) {
        return NOTES_OK;
    }

    size_t at = cursor_byte_offset(ed);
    if (at == 0u || at > ed->length) {
        return NOTES_ERR_RANGE;
    }

    unsigned char removed = (unsigned char)ed->bytes[at - 1u];
    for (size_t i = at - 1u; i + 1u <= ed->length; i++) {
        ed->bytes[i] = ed->bytes[i + 1u];
    }
    ed->length--;
    ed->bytes[ed->length] = '\0';

    if (removed == (unsigned char)'\n') {
        if (ed->cursor.line > 0u) {
            ed->cursor.line--;
            uint32_t prev_start = ed->line_starts[ed->cursor.line];
            ed->cursor.column   = (uint32_t)((at - 1u) - prev_start);
        }
    } else if (ed->cursor.column > 0u) {
        ed->cursor.column--;
    }

    notes_editor_reindex_lines(ed);
    cursor_clamp(ed);
    return NOTES_OK;
}

notes_status_t notes_editor_delete_forward(notes_editor_t *ed)
{
    if (ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (ed->length == 0u) {
        return NOTES_OK;
    }

    size_t at = cursor_byte_offset(ed);
    if (at >= ed->length) {
        return NOTES_OK;
    }

    for (size_t i = at; i + 1u <= ed->length; i++) {
        ed->bytes[i] = ed->bytes[i + 1u];
    }
    ed->length--;
    ed->bytes[ed->length] = '\0';
    notes_editor_reindex_lines(ed);
    cursor_clamp(ed);
    return NOTES_OK;
}

const notes_selection_t *notes_editor_selection(const notes_editor_t *ed)
{
    return (ed != NULL) ? &ed->selection : NULL;
}

notes_status_t notes_editor_clear_selection(notes_editor_t *ed)
{
    if (ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    ed->selection.active = false;
    return NOTES_OK;
}

notes_status_t notes_editor_select_all(notes_editor_t *ed)
{
    if (ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    ed->selection.active      = true;
    ed->selection.anchor.line = 0u;
    ed->selection.anchor.column = 0u;
    if (ed->line_count == 0u) {
        ed->selection.caret.line   = 0u;
        ed->selection.caret.column = 0u;
        return NOTES_OK;
    }
    uint32_t last = ed->line_count - 1u;
    ed->selection.caret.line = last;
    ed->selection.caret.column =
        line_end_offset(ed, last) - ed->line_starts[last];
    return NOTES_OK;
}
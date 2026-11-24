/* MDE Notes — undo/redo por instantáneas (stub SA-27) */

#include "undo.h"

static void snapshot_copy(notes_editor_t *dst, const notes_editor_t *src)
{
    dst->length    = src->length;
    dst->line_count = src->line_count;
    dst->cursor    = src->cursor;
    dst->selection = src->selection;

    for (size_t i = 0; i <= src->length; i++) {
        dst->bytes[i] = src->bytes[i];
    }
    for (uint32_t i = 0; i < src->line_count; i++) {
        dst->line_starts[i] = src->line_starts[i];
    }
}

notes_status_t notes_undo_init(notes_undo_stack_t *st)
{
    if (st == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    notes_undo_reset(st);
    return NOTES_OK;
}

void notes_undo_reset(notes_undo_stack_t *st)
{
    if (st == NULL) {
        return;
    }
    st->undo_top   = 0u;
    st->undo_count = 0u;
    st->in_replay  = false;
}

notes_status_t notes_undo_checkpoint(notes_undo_stack_t *st, const notes_editor_t *ed)
{
    if (st == NULL || ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (st->in_replay) {
        return NOTES_OK;
    }

    /* Descartar ramas de redo tras una nueva edición. */
    if (st->undo_top + 1u < st->undo_count) {
        st->undo_count = st->undo_top + 1u;
    }

    if (st->undo_count < NOTES_UNDO_MAX_LEVELS) {
        snapshot_copy(&st->snapshots[st->undo_count], ed);
        st->undo_top   = st->undo_count;
        st->undo_count++;
        return NOTES_OK;
    }

    for (uint32_t i = 1u; i < NOTES_UNDO_MAX_LEVELS; i++) {
        st->snapshots[i - 1u] = st->snapshots[i];
    }
    snapshot_copy(&st->snapshots[NOTES_UNDO_MAX_LEVELS - 1u], ed);
    st->undo_top   = NOTES_UNDO_MAX_LEVELS - 1u;
    st->undo_count = NOTES_UNDO_MAX_LEVELS;
    return NOTES_OK;
}

bool notes_undo_can_undo(const notes_undo_stack_t *st)
{
    return (st != NULL && st->undo_count > 1u && st->undo_top > 0u);
}

bool notes_undo_can_redo(const notes_undo_stack_t *st)
{
    return (st != NULL && st->undo_top + 1u < st->undo_count);
}

notes_status_t notes_undo_apply(notes_undo_stack_t *st, notes_editor_t *ed)
{
    if (st == NULL || ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (!notes_undo_can_undo(st)) {
        return NOTES_ERR_BUSY;
    }

    st->in_replay = true;
    st->undo_top--;
    snapshot_copy(ed, &st->snapshots[st->undo_top]);
    st->in_replay = false;
    return NOTES_OK;
}

notes_status_t notes_redo_apply(notes_undo_stack_t *st, notes_editor_t *ed)
{
    if (st == NULL || ed == NULL) {
        return NOTES_ERR_INVALID_ARG;
    }
    if (!notes_undo_can_redo(st)) {
        return NOTES_ERR_BUSY;
    }

    st->in_replay = true;
    st->undo_top++;
    snapshot_copy(ed, &st->snapshots[st->undo_top]);
    st->in_replay = false;
    return NOTES_OK;
}
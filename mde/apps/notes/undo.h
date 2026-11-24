#ifndef MDE_NOTES_UNDO_H
#define MDE_NOTES_UNDO_H

#include "editor.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Pila de instantáneas completas del documento (stub SA-27).
 * Sustituible por comandos atómicos cuando el runtime de usuario esté listo.
 */
#define NOTES_UNDO_MAX_LEVELS 16u

typedef struct notes_undo_stack {
    notes_editor_t snapshots[NOTES_UNDO_MAX_LEVELS];
    uint32_t       undo_top;   /* índice del snapshot actual */
    uint32_t       undo_count; /* niveles almacenados (1..NOTES_UNDO_MAX_LEVELS) */
    bool           in_replay;  /* evita reentrada al aplicar undo/redo */
} notes_undo_stack_t;

notes_status_t notes_undo_init(notes_undo_stack_t *st);
void           notes_undo_reset(notes_undo_stack_t *st);

notes_status_t notes_undo_checkpoint(notes_undo_stack_t *st, const notes_editor_t *ed);

notes_status_t notes_undo_apply(notes_undo_stack_t *st, notes_editor_t *ed);
notes_status_t notes_redo_apply(notes_undo_stack_t *st, notes_editor_t *ed);

bool notes_undo_can_undo(const notes_undo_stack_t *st);
bool notes_undo_can_redo(const notes_undo_stack_t *st);

#endif /* MDE_NOTES_UNDO_H */
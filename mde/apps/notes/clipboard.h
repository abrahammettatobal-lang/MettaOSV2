#ifndef MDE_NOTES_CLIPBOARD_H
#define MDE_NOTES_CLIPBOARD_H

#include "notes.h"

/* Stub: portapapeles del escritorio (SA-22 / compositor). */

notes_status_t notes_clipboard_set_text(const char *text, size_t len);
notes_status_t notes_clipboard_get_text(char *buf, size_t buf_cap, size_t *out_len);

#endif /* MDE_NOTES_CLIPBOARD_H */
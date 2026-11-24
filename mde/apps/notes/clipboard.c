/* MDE Notes — portapapeles (stub; pendiente integración MDE) */

#include "clipboard.h"

notes_status_t notes_clipboard_set_text(const char *text, size_t len)
{
    (void)text;
    (void)len;
    return NOTES_ERR_UNINITIALIZED;
}

notes_status_t notes_clipboard_get_text(char *buf, size_t buf_cap, size_t *out_len)
{
    (void)buf;
    (void)buf_cap;
    (void)out_len;
    return NOTES_ERR_UNINITIALIZED;
}
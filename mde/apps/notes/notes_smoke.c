/* Prueba host-only del núcleo lógico de MDE Notes (gcc -std=c11, no kernel). */

#include "editor.h"
#include "notes.h"
#include "vfs_io.h"

#include <stdio.h>
#include <string.h>

typedef struct mock_file {
    char     data[1024];
    size_t   len;
    size_t   pos;
    int      flags;
} mock_file_t;

static mock_file_t g_mock;

static notes_status_t mock_open(const char *path, int flags, notes_vfs_handle_t *out)
{
    (void)path;
    g_mock.pos   = 0;
    g_mock.flags = flags;
    if ((flags & NOTES_VFS_O_TRUNC) != 0) {
        g_mock.len      = 0;
        g_mock.data[0]  = '\0';
    }
    *out = (notes_vfs_handle_t)&g_mock;
    return NOTES_OK;
}

static notes_status_t mock_close(notes_vfs_handle_t h)
{
    (void)h;
    return NOTES_OK;
}

static notes_status_t mock_read(notes_vfs_handle_t h, void *buf, size_t len, size_t *out_read)
{
    mock_file_t *f = (mock_file_t *)h;
    size_t avail;

    if (f->pos >= f->len) {
        *out_read = 0;
        return NOTES_OK;
    }
    avail = f->len - f->pos;
    if (len > avail) {
        len = avail;
    }
    memcpy(buf, f->data + f->pos, len);
    f->pos += len;
    *out_read = len;
    return NOTES_OK;
}

static notes_status_t mock_write(notes_vfs_handle_t h, const void *buf, size_t len, size_t *out_written)
{
    mock_file_t *f = (mock_file_t *)h;

    if (f->len + len >= sizeof(f->data)) {
        return NOTES_ERR_TOO_LARGE;
    }
    memcpy(f->data + f->len, buf, len);
    f->len += len;
    f->pos  = f->len;
    *out_written = len;
    return NOTES_OK;
}

int main(void)
{
    static const notes_vfs_ops_t ops = {
        .open  = mock_open,
        .close = mock_close,
        .read  = mock_read,
        .write = mock_write,
        .truncate = NULL,
    };

    notes_app_t app;
    notes_status_t st;
    int ok = 0;

    notes_vfs_bind(&ops);
    st = notes_app_init(&app);
    if (st != NOTES_OK) {
        return 1;
    }

    st = notes_app_insert_text(&app, "linea A\n", 8);
    if (st != NOTES_OK) {
        return 2;
    }
    st = notes_app_insert_text(&app, "linea B", 7);
    if (st != NOTES_OK) {
        return 3;
    }

    if (notes_editor_line_count(notes_app_editor(&app)) != 2u) {
        return 4;
    }

    st = notes_app_save_as(&app, "/home/notas.txt");
    if (st != NOTES_OK) {
        return 5;
    }

    st = notes_app_insert_text(&app, "!", 1);
    if (st != NOTES_OK) {
        return 6;
    }

    if (!notes_app_can_undo(&app)) {
        return 7;
    }

    st = notes_app_undo(&app);
    if (st != NOTES_OK) {
        return 8;
    }

    if (notes_editor_length(notes_app_editor(&app)) != 15u) {
        return 9;
    }

    st = notes_app_new_document(&app);
    if (st != NOTES_OK) {
        return 10;
    }

    st = notes_app_open_path(&app, "/home/notas.txt");
    if (st != NOTES_OK) {
        return 11;
    }

    if (strncmp(g_mock.data, "linea A\nlinea B", 15) != 0) {
        return 12;
    }

    ok = 1;
    printf("notes_smoke: PASS (%d checks)\n", ok);
    notes_app_shutdown(&app);
    return 0;
}
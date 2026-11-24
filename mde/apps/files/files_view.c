#include "files_view.h"

#include <stddef.h>
#include <stdint.h>

static void paint_fill(const mde_files_painter *p, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c)
{
    if (p != NULL && p->fill_rect != NULL) {
        p->fill_rect(p->ctx, x, y, w, h, c);
    }
}

static void paint_stroke(const mde_files_painter *p, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c)
{
    if (p != NULL && p->stroke_rect != NULL) {
        p->stroke_rect(p->ctx, x, y, w, h, c);
    }
}

static void paint_text(const mde_files_painter *p, int32_t x, int32_t y, const char *text, uint32_t c)
{
    if (p != NULL && p->draw_text != NULL && text != NULL) {
        p->draw_text(p->ctx, x, y, text, c);
    }
}

static void rect_set(mde_files_rect *r, int32_t x, int32_t y, int32_t w, int32_t h)
{
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
}

void mde_files_view_layout_compute(mde_files_view_layout *lay, int32_t cw, int32_t ch)
{
    if (lay == NULL) {
        return;
    }

    mde_files_chrome_light(&lay->chrome);

    if (cw < 320) {
        cw = 320;
    }
    if (ch < 200) {
        ch = 200;
    }

    rect_set(&lay->client, 0, 0, cw, ch);
    rect_set(&lay->sidebar, 0, 0, MDE_FILES_SIDEBAR_WIDTH, ch);
    rect_set(
        &lay->toolbar,
        MDE_FILES_SIDEBAR_WIDTH,
        0,
        cw - MDE_FILES_SIDEBAR_WIDTH,
        MDE_FILES_TOOLBAR_HEIGHT);
    rect_set(
        &lay->path_field,
        lay->toolbar.x + MDE_FILES_PATH_INSET,
        lay->toolbar.y,
        lay->toolbar.w - MDE_FILES_PATH_INSET * 2,
        lay->toolbar.h);
    rect_set(
        &lay->list_header,
        MDE_FILES_SIDEBAR_WIDTH,
        MDE_FILES_TOOLBAR_HEIGHT,
        cw - MDE_FILES_SIDEBAR_WIDTH,
        MDE_FILES_LIST_HEADER_HEIGHT);
    rect_set(
        &lay->list_body,
        lay->list_header.x,
        lay->list_header.y + lay->list_header.h,
        lay->list_header.w,
        ch - lay->list_header.y - lay->list_header.h);

    if (lay->list_body.h > 0) {
        lay->visible_rows = lay->list_body.h / MDE_FILES_ROW_HEIGHT;
    } else {
        lay->visible_rows = 0;
    }
}

static void paint_sidebar(const mde_files_view_layout *lay, const mde_files_painter *p)
{
    const mde_files_chrome *c = &lay->chrome;
    paint_fill(p, lay->sidebar.x, lay->sidebar.y, lay->sidebar.w, lay->sidebar.h, c->sidebar_bg);
    paint_stroke(
        p,
        lay->sidebar.x + lay->sidebar.w - 1,
        lay->sidebar.y,
        1,
        lay->sidebar.h,
        c->list_divider);

    paint_text(p, lay->sidebar.x + 12, lay->sidebar.y + 10, "Favoritos", c->window.title_text);
    paint_text(p, lay->sidebar.x + 12, lay->sidebar.y + 36, "Inicio", c->folder_tint);
    paint_text(p, lay->sidebar.x + 12, lay->sidebar.y + 58, "Documentos", c->file_tint);
}

static void paint_toolbar(const mde_files_view_layout *lay, const mde_files_model *model, const mde_files_painter *p)
{
    const mde_files_chrome *c = &lay->chrome;

    paint_fill(p, lay->toolbar.x, lay->toolbar.y, lay->toolbar.w, lay->toolbar.h, c->toolbar.background);
    paint_fill(
        p,
        lay->path_field.x,
        lay->path_field.y + 4,
        lay->path_field.w,
        lay->path_field.h - 8,
        c->list_bg);
    paint_stroke(
        p,
        lay->path_field.x,
        lay->path_field.y + 4,
        lay->path_field.w,
        lay->path_field.h - 8,
        c->list_divider);

    uint32_t nav_color = c->button.label;
    if (model != NULL) {
        if (!mde_files_model_can_back(model)) {
            nav_color = c->button.label_disabled;
        }
        paint_text(p, lay->toolbar.x + 8, lay->toolbar.y + 8, "<", nav_color);

        nav_color = c->button.label;
        if (!mde_files_model_can_forward(model)) {
            nav_color = c->button.label_disabled;
        }
        paint_text(p, lay->toolbar.x + 28, lay->toolbar.y + 8, ">", nav_color);
        paint_text(p, lay->toolbar.x + 48, lay->toolbar.y + 8, "^", c->button.label);
    }

    if (model != NULL) {
        paint_text(
            p,
            lay->path_field.x + 8,
            lay->path_field.y + 8,
            model->current_path,
            c->window.title_text);
    }
}

static void format_size_human(uint64_t bytes, char *buf, size_t cap)
{
    if (buf == NULL || cap == 0) {
        return;
    }

    if ((bytes >> 20) > 0) {
        uint64_t mb = (bytes + (1u << 19)) >> 20;
        size_t i = 0;
        const char *s = " MiB";
        uint64_t v = mb;
        char tmp[16];
        size_t t = 0;
        if (v == 0) {
            tmp[t++] = '0';
        } else {
            while (v > 0 && t + 1 < sizeof tmp) {
                tmp[t++] = (char)('0' + (v % 10));
                v /= 10;
            }
        }
        while (t > 0 && i + 1 < cap) {
            buf[i++] = tmp[--t];
        }
        size_t si = 0;
        while (s[si] != '\0' && i + 1 < cap) {
            buf[i++] = s[si++];
        }
        buf[i] = '\0';
        return;
    }

    if ((bytes >> 10) > 0) {
        uint64_t kb = (bytes + 512) >> 10;
        size_t i = 0;
        const char *s = " KiB";
        char tmp[16];
        size_t t = 0;
        uint64_t v = kb;
        if (v == 0) {
            tmp[t++] = '0';
        } else {
            while (v > 0 && t + 1 < sizeof tmp) {
                tmp[t++] = (char)('0' + (v % 10));
                v /= 10;
            }
        }
        while (t > 0 && i + 1 < cap) {
            buf[i++] = tmp[--t];
        }
        size_t si = 0;
        while (s[si] != '\0' && i + 1 < cap) {
            buf[i++] = s[si++];
        }
        buf[i] = '\0';
        return;
    }

    size_t i = 0;
    char tmp[16];
    size_t t = 0;
    uint64_t v = bytes;
    if (v == 0) {
        tmp[t++] = '0';
    } else {
        while (v > 0 && t + 1 < sizeof tmp) {
            tmp[t++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    while (t > 0 && i + 1 < cap) {
        buf[i++] = tmp[--t];
    }
    if (i + 3 < cap) {
        buf[i++] = ' ';
        buf[i++] = 'B';
    }
    buf[i] = '\0';
}

static void paint_list(const mde_files_view_layout *lay, const mde_files_model *model, const mde_files_painter *p)
{
    const mde_files_chrome *c = &lay->chrome;

    paint_fill(p, lay->list_header.x, lay->list_header.y, lay->list_header.w, lay->list_header.h, c->list_header_bg);
    paint_text(p, lay->list_header.x + MDE_FILES_ICON_COL_WIDTH, lay->list_header.y + 5, "Nombre", c->file_tint);
    int32_t sx = lay->list_header.x + lay->list_header.w - MDE_FILES_SIZE_COL_WIDTH - MDE_FILES_DATE_COL_WIDTH;
    paint_text(p, sx, lay->list_header.y + 5, "Tamaño", c->file_tint);

    paint_fill(p, lay->list_body.x, lay->list_body.y, lay->list_body.w, lay->list_body.h, c->list_bg);

    if (model == NULL) {
        return;
    }

    int32_t row_y = lay->list_body.y;
    size_t start = (size_t)model->scroll_row;
    if (start >= model->entry_count) {
        start = 0;
    }

    for (int32_t vis = 0; vis < lay->visible_rows; vis++) {
        size_t idx = start + (size_t)vis;
        if (idx >= model->entry_count) {
            break;
        }

        const mde_files_dirent_t *ent = &model->entries[idx];
        int selected = model->selected_index == (int32_t)idx;

        uint32_t row_bg = c->list_bg;
        if (selected) {
            row_bg = c->list_row_selected;
        }
        paint_fill(p, lay->list_body.x, row_y, lay->list_body.w, MDE_FILES_ROW_HEIGHT, row_bg);

        uint32_t name_color = ((ent->flags & MDE_FILES_ENT_DIR) != 0) ? c->folder_tint : c->window.title_text;
        paint_text(p, lay->list_body.x + MDE_FILES_ICON_COL_WIDTH, row_y + 6, ent->name, name_color);

        char size_buf[24];
        if ((ent->flags & MDE_FILES_ENT_DIR) != 0) {
            size_buf[0] = '—';
            size_buf[1] = '\0';
        } else {
            format_size_human(ent->size_bytes, size_buf, sizeof size_buf);
        }
        paint_text(p, sx, row_y + 6, size_buf, c->file_tint);

        row_y += MDE_FILES_ROW_HEIGHT;
    }
}

void mde_files_view_paint(
    const mde_files_view_layout *lay,
    const mde_files_model *model,
    const mde_files_painter *painter)
{
    if (lay == NULL) {
        return;
    }

    const mde_files_chrome *c = &lay->chrome;
    paint_fill(p, lay->client.x, lay->client.y, lay->client.w, lay->client.h, c->window.client_bg);
    paint_stroke(p, lay->client.x, lay->client.y, lay->client.w, lay->client.h, c->window.border);

    paint_sidebar(lay, painter);
    paint_toolbar(lay, model, painter);
    paint_list(lay, model, painter);
}

int32_t mde_files_view_hit_row(const mde_files_view_layout *lay, const mde_files_model *model, int32_t y)
{
    if (lay == NULL || model == NULL) {
        return -1;
    }
    if (y < lay->list_body.y || y >= lay->list_body.y + lay->list_body.h) {
        return -1;
    }

    int32_t rel = (y - lay->list_body.y) / MDE_FILES_ROW_HEIGHT;
    size_t idx = (size_t)model->scroll_row + (size_t)rel;
    if (idx >= model->entry_count) {
        return -1;
    }
    return (int32_t)idx;
}
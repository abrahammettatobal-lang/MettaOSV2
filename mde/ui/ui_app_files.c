/*
 * MettaOS — Archivos: explorador del sistema de archivos en RAM.
 */
#include "ui_apps_common.h"

#define SIDE_X   8
#define SIDE_W   184
#define CONT_X   (SIDE_X + SIDE_W + 14)
#define CONT_Y   58
#define CELL_W   104
#define CELL_H   100
#define ROW_H    38
#define HIST     16

typedef struct {
    int     dir;
    int     back[HIST], nback;
    int     fwd[HIST], nfwd;
    int     sel;
    int32_t scroll;
    int     hover_side;
    int     hover_btn;
    int     list;
} files_t;

static files_t g_f[UI_MAX_WIN];

enum { M_OPEN = 1, M_TRASH, M_NEWDIR, M_NEWDOC, M_EMPTY, M_RESTORE };

static int side_dir(int k, const char **label)
{
    static const char *names[] = { "Escritorio", "Documentos", "Descargas", "Imágenes",
                                   "Música", "Aplicaciones", "Papelera" };
    if (k < 0 || k >= 7)
        return -1;
    if (label)
        *label = names[k];
    if (k == 5)
        return fs_apps_dir();
    return fs_find(fs_home(), names[k]);
}

static void set_title(ui_window_t *w, files_t *s)
{
    fs_node_t *n = fs_node(s->dir);

    if (s->dir == fs_home())
        ui_strlcpy(w->title, "usuario", sizeof(w->title));
    else if (n && n->name[0])
        ui_strlcpy(w->title, n->name, sizeof(w->title));
    else
        ui_strlcpy(w->title, "MettaOS", sizeof(w->title));
}

static void navigate(ui_window_t *w, files_t *s, int dir, int push)
{
    if (!fs_node(dir) || fs_node(dir)->type != FS_DIR)
        return;
    if (push && dir != s->dir) {
        if (s->nback == HIST) {
            int i;
            for (i = 1; i < HIST; i++)
                s->back[i - 1] = s->back[i];
            s->nback--;
        }
        s->back[s->nback++] = s->dir;
        s->nfwd = 0;
    }
    s->dir = dir;
    s->sel = -1;
    s->scroll = 0;
    set_title(w, s);
    ui_win_invalidate(w);
}

static void files_open(ui_window_t *w, int slot, const char *arg)
{
    files_t *s = &g_f[slot];
    int d = arg ? fs_lookup(fs_root(), arg) : fs_home();

    memset(s, 0, sizeof(*s));
    s->sel = -1;
    s->hover_side = -1;
    s->list = 1;
    s->hover_btn = -1;
    s->dir = fs_home();
    navigate(w, s, d >= 0 ? d : fs_home(), 0);
}

static int cols_of(const ui_window_t *w)
{
    int c = (w->w - CONT_X - 12) / CELL_W;
    return c < 1 ? 1 : c;
}

static int item_at(const ui_window_t *w, const files_t *s, int32_t lx, int32_t ly)
{
    int cols = cols_of(w), k, f = -1, i = 0;

    if (lx < CONT_X || ly < CONT_Y || ly > w->h - 30)
        return -1;
    k = s->list ? (ly-CONT_Y+s->scroll)/ROW_H
                : ((ly - CONT_Y + s->scroll) / CELL_H) * cols + (lx - CONT_X) / CELL_W;
    if (lx >= w->w-10 || (!s->list && (lx - CONT_X) / CELL_W >= cols))
        return -1;
    while ((f = fs_child_next(s->dir, f)) >= 0) {
        if (i == k)
            return f;
        i++;
    }
    return -1;
}

static void files_draw(ui_window_t *w, int slot, ui_canvas_t *c, int32_t ox, int32_t oy, int focused)
{
    files_t *s = &g_f[slot];
    int k, f = -1, i = 0, cols = cols_of(w);
    const char *label;
    char status[48], num[12];
    int32_t cy0 = c->cy0, cy1 = c->cy1;

    (void)focused;
    /* barra lateral */
    ui_text(c, ox + SIDE_X + 14, oy + 46, "Favoritos", UI_FONT_SMALL, UI_TEXT_2, 255u);
    for (k = 0; k < 7; k++) {
        int d = side_dir(k, &label);
        int32_t iy = oy + 64 + k * 30;
        int icon = (k == 6) ? UI_ICON_TRASH : (k == 5 ? UI_ICON_ABOUT : UI_ICON_FOLDER);

        if (d == s->dir)
            ui_rrect(c, (float)(ox + SIDE_X + 8), (float)iy, (float)(SIDE_W - 16), 28.0f, 8.0f,
                     0x000000u, 26u);
        else if (k == s->hover_side)
            ui_rrect(c, (float)(ox + SIDE_X + 8), (float)iy, (float)(SIDE_W - 16), 28.0f, 8.0f,
                     0x000000u, 12u);
        ui_icon_draw(c, icon, (float)(ox + SIDE_X + 16), (float)(iy + 4), 20.0f);
        ui_text(c, ox + SIDE_X + 44, iy + 5, label, UI_FONT_UI, UI_TEXT, 255u);
    }

    /* barra de herramientas */
    app_pill(c, ox + CONT_X, oy + 10, 64, 30, s->hover_btn == 0 || s->hover_btn == 1, 0);
    ui_fill(c, ox + CONT_X + 32, oy + 16, 1, 18, 0x000000u, 25u);
    app_chevron(c, (float)(ox + CONT_X + 16), (float)(oy + 25), 1, s->nback ? 0x3A3A3Cu : 0xC0C0C5u, 255u);
    app_chevron(c, (float)(ox + CONT_X + 48), (float)(oy + 25), 0, s->nfwd ? 0x3A3A3Cu : 0xC0C0C5u, 255u);
    ui_text_fit(c, ox + CONT_X + 78, oy + 15, w->title, UI_FONT_UI_BOLD, UI_TEXT, 255u, w->w - CONT_X - 174);
    app_pill(c, ox+w->w-96, oy+10, 36, 30, s->hover_btn == 3, 0);
    for (int line=0; line<(s->list ? 2 : 3); line++) {
        int yy=oy+20+line*5;
        ui_fill(c, ox+w->w-85, yy, s->list ? 5 : 14, s->list ? 4 : 2, UI_TEXT, 255);
        if (s->list) ui_fill(c, ox+w->w-77, yy, 5, 4, UI_TEXT, 255);
    }
    app_pill(c, ox + w->w - 52, oy + 10, 36, 30, s->hover_btn == 2, 0);
    app_plus(c, (float)(ox + w->w - 34), (float)(oy + 25), 6.0f, 0x3A3A3Cu, 255u);

    /* contenido */
    c->cy0 = ui_max(cy0, oy + CONT_Y - 4);
    c->cy1 = ui_min(cy1, oy + w->h - 30);
    while ((f = fs_child_next(s->dir, f)) >= 0) {
        fs_node_t *nd = fs_node(f);
        if (s->list) {
            int x=ox+CONT_X, y=oy+CONT_Y+i++*ROW_H-s->scroll, width=w->w-CONT_X-12;
            if (y+ROW_H <= c->cy0 || y >= c->cy1) continue;
            if (s->sel == f) ui_rrect(c, x, y, width, ROW_H-2, 4, 0xD9EAE3, 255);
            ui_icon_draw(c, fs_icon(f), x+6, y+5, 27);
            static const char *const kinds[] = {"Carpeta", "Texto", "AppImage", "Aplicación"};
            ui_text_fit(c, x+45, y+9, nd->name, UI_FONT_UI, UI_TEXT, 255, width-136);
            ui_text_fit(c, x+width-81, y+10, kinds[nd->type], UI_FONT_SMALL, UI_TEXT_2, 255, 76);
            ui_fill(c, x+44, y+ROW_H-1, width-48, 1, UI_SEPARATOR, 130);
            continue;
        }
        int32_t cx = ox + CONT_X + (i % cols) * CELL_W;
        int32_t cyy = oy + CONT_Y + (i / cols) * CELL_H - s->scroll;
        int32_t tw = ui_min(ui_text_width(nd->name, UI_FONT_SMALL), CELL_W - 8);
        uint32_t fg = UI_TEXT;

        if (f == s->sel) {
            ui_rrect(c, (float)(cx + 18), (float)(cyy + 2), 68.0f, 66.0f, 10.0f, 0x000000u, 22u);
            ui_rrect(c, (float)(cx + CELL_W / 2 - tw / 2 - 6), (float)(cyy + 72), (float)(tw + 12),
                     19.0f, 6.0f, UI_ACCENT, 255u);
            fg = 0xFFFFFFu;
        }
        ui_icon_draw(c, fs_icon(f), (float)(cx + 22), (float)(cyy + 6), 60.0f);
        ui_text_fit(c, cx + CELL_W / 2 - tw / 2, cyy + 73, nd->name, UI_FONT_SMALL, fg, 255u, CELL_W - 8);
        i++;
    }
    if (i == 0)
        ui_text_center(c, ox + CONT_X + (w->w - CONT_X) / 2, oy + w->h / 2 - 10, "Carpeta vacía",
                       UI_FONT_UI, 0xAEAEB2u, 255u);
    c->cy0 = cy0;
    c->cy1 = cy1;

    /* estado */
    ui_fill(c, ox + CONT_X, oy + w->h - 30, w->w - CONT_X, 1, 0x000000u, 18u);
    ui_itoa(i, num);
    ui_strlcpy(status, num, sizeof(status));
    ui_strlcat(status, i == 1 ? " elemento" : " elementos", sizeof(status));
    char path[160];
    fs_path(s->dir, path, sizeof(path));
    ui_text_fit(c, ox+CONT_X+6, oy+w->h-24, path, UI_FONT_SMALL, UI_TEXT_2, 255, w->w-CONT_X-112);
    ui_text(c, ox+w->w-16-ui_text_width(status,UI_FONT_SMALL), oy+w->h-24, status, UI_FONT_SMALL, UI_TEXT_2, 255);
}

static void menu_cb(int id, void *ctx)
{
    int slot = (int)(intptr_t)ctx;
    files_t *s = &g_f[slot];
    char name[48];
    int f;
    ui_window_t *w = ui_window_by_slot(slot);

    if (!w)
        return;
    switch (id) {
    case M_OPEN:
        if (fs_node(s->sel) && fs_node(s->sel)->type == FS_DIR)
            navigate(w, s, s->sel, 1);
        else
            fs_open(s->sel);
        break;
    case M_TRASH:
        if (s->dir == fs_trash())
            fs_remove(s->sel);
        else
            fs_move(s->sel, fs_trash());
        s->sel = -1;
        break;
    case M_NEWDIR:
        fs_unique_name(s->dir, "Carpeta nueva", "", name, sizeof(name));
        s->sel = fs_create(s->dir, name, FS_DIR);
        break;
    case M_NEWDOC:
        fs_unique_name(s->dir, "Documento", ".txt", name, sizeof(name));
        s->sel = fs_create(s->dir, name, FS_TEXT);
        break;
    case M_EMPTY:
        while ((f = fs_child_next(fs_trash(), -1)) >= 0)
            fs_remove(f);
        s->sel = -1;
        break;
    case M_RESTORE:
        fs_move(s->sel, fs_docs());
        s->sel = -1;
        break;
    default:
        break;
    }
    ui_invalidate(0, 0, ui_screen_w(), ui_screen_h());
}

static void open_sel(ui_window_t *w, files_t *s)
{
    fs_node_t *n = fs_node(s->sel);

    if (!n)
        return;
    if (n->type == FS_DIR)
        navigate(w, s, s->sel, 1);
    else
        fs_open(s->sel);
}

static void files_mouse(ui_window_t *w, int slot, int type, int32_t lx, int32_t ly, int btn, int clicks)
{
    files_t *s = &g_f[slot];
    int hs = -1, hb = -1;

    if (type == UI_M_LEAVE) {
        if (s->hover_side != -1 || s->hover_btn != -1) {
            s->hover_side = -1;
            s->hover_btn = -1;
            ui_win_invalidate(w);
        }
        return;
    }
    if (lx >= SIDE_X && lx < SIDE_X + SIDE_W && ly >= 64 && ly < 64 + 7 * 30)
        hs = (ly - 64) / 30;
    if (app_hit(lx, ly, CONT_X, 10, 32, 30))
        hb = 0;
    else if (app_hit(lx, ly, CONT_X + 32, 10, 32, 30))
        hb = 1;
    else if (app_hit(lx, ly, w->w - 52, 10, 36, 30))
        hb = 2;
    else if (app_hit(lx, ly, w->w-96, 10, 36, 30))
        hb = 3;

    if (type == UI_M_HOVER) {
        if (hs != s->hover_side || hb != s->hover_btn) {
            s->hover_side = hs;
            s->hover_btn = hb;
            ui_win_invalidate(w);
        }
        return;
    }
    if (type != UI_M_DOWN)
        return;

    if (hb >= 0 && btn == INPUT_BTN_LEFT) {
        ui_consume();
        if (hb == 0 && s->nback > 0) {
            s->fwd[s->nfwd < HIST ? s->nfwd++ : HIST - 1] = s->dir;
            navigate(w, s, s->back[--s->nback], 0);
        } else if (hb == 1 && s->nfwd > 0) {
            s->back[s->nback < HIST ? s->nback++ : HIST - 1] = s->dir;
            navigate(w, s, s->fwd[--s->nfwd], 0);
        } else if (hb == 2) {
            menu_cb(M_NEWDIR, (void *)(intptr_t)slot);
        } else if (hb == 3) {
            s->list = !s->list;
            s->scroll = 0;
            ui_win_invalidate(w);
        }
        return;
    }
    if (hs >= 0) {
        ui_consume();
        if (btn == INPUT_BTN_LEFT)
            navigate(w, s, side_dir(hs, NULL), 1);
        return;
    }
    if (lx >= CONT_X && ly >= CONT_Y) {
        int f = item_at(w, s, lx, ly);

        ui_consume();
        s->sel = f;
        ui_win_invalidate(w);
        if (btn == INPUT_BTN_LEFT && clicks >= 2 && f >= 0) {
            open_sel(w, s);
        } else if (btn == INPUT_BTN_RIGHT) {
            ui_menu_item_t items[6];
            int n = 0;
            if (f >= 0) {
                items[n++] = (ui_menu_item_t){ "Abrir", M_OPEN, 0, NULL };
                items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
                if (s->dir == fs_trash()) {
                    items[n++] = (ui_menu_item_t){ "Devolver a Documentos", M_RESTORE, 0, NULL };
                    items[n++] = (ui_menu_item_t){ "Eliminar definitivamente", M_TRASH, 0, NULL };
                } else {
                    items[n++] = (ui_menu_item_t){ "Mover a la Papelera", M_TRASH, 0, "Supr" };
                }
            } else if (s->dir == fs_trash()) {
                items[n++] = (ui_menu_item_t){ "Vaciar Papelera", M_EMPTY, fs_count_children(fs_trash()) == 0, NULL };
            } else {
                items[n++] = (ui_menu_item_t){ "Nueva carpeta", M_NEWDIR, 0, NULL };
                items[n++] = (ui_menu_item_t){ "Nuevo documento", M_NEWDOC, 0, NULL };
            }
            ui_menu_popup(w->x + lx, w->y + ly, items, n, menu_cb, (void *)(intptr_t)slot);
        }
    }
}

static void files_key(ui_window_t *w, int slot, const input_event_t *ev, const char *text)
{
    files_t *s = &g_f[slot];
    int n = fs_count_children(s->dir), idx = -1, i = 0, f = -1, cols = cols_of(w);
    (void)text;

    while ((f = fs_child_next(s->dir, f)) >= 0) {
        if (f == s->sel)
            idx = i;
        i++;
    }
    switch (ev->u.key.keycode) {
    case INPUT_KEY_ENTER:
        open_sel(w, s);
        return;
    case INPUT_KEY_DELETE:
        if (s->sel >= 0)
            menu_cb(M_TRASH, (void *)(intptr_t)slot);
        return;
    case INPUT_KEY_BACKSPACE:
        if (s->nback > 0) {
            s->fwd[s->nfwd < HIST ? s->nfwd++ : HIST - 1] = s->dir;
            navigate(w, s, s->back[--s->nback], 0);
        }
        return;
    case INPUT_KEY_RIGHT:
        idx = idx < 0 ? 0 : idx + 1;
        break;
    case INPUT_KEY_LEFT:
        idx = idx <= 0 ? 0 : idx - 1;
        break;
    case INPUT_KEY_DOWN:
        idx = idx < 0 ? 0 : idx + (s->list ? 1 : cols);
        break;
    case INPUT_KEY_UP:
        idx = idx < 0 ? 0 : idx - (s->list ? 1 : cols);
        break;
    default:
        return;
    }
    if (n == 0)
        return;
    idx = ui_clampi(idx, 0, n - 1);
    f = -1;
    i = 0;
    while ((f = fs_child_next(s->dir, f)) >= 0) {
        if (i++ == idx) {
            s->sel = f;
            break;
        }
    }
    ui_win_invalidate(w);
}

static void files_wheel(ui_window_t *w, int slot, int delta)
{
    files_t *s = &g_f[slot];
    int cols = s->list ? 1 : cols_of(w);
    int rows = (fs_count_children(s->dir) + cols - 1) / cols;
    int32_t maxs = ui_max(0, rows * (s->list ? ROW_H : CELL_H) - (w->h - CONT_Y - 34));

    s->scroll = ui_clampi(s->scroll + delta * 40, 0, maxs);
    ui_win_invalidate(w);
}

static int files_glass(ui_window_t *w, int slot, ui_rect_t *out, int max)
{
    (void)slot;
    if (max < 1)
        return 0;
    out[0] = (ui_rect_t){ SIDE_X, 8, SIDE_W, w->h - 16 };
    return 1;
}

const ui_app_t app_files = {
    "Archivos", UI_ICON_FILES, 780, 500, 520, 320, 0xFFFFFFu, 0, 0, 0, 0,
    files_open, files_draw, files_mouse, files_key, files_wheel, NULL, files_glass, NULL,
};

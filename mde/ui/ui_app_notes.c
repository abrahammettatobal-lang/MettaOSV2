/*
 * MettaOS — Notas: editor con ajuste de línea; las notas son .txt en Documentos.
 */
#include "ui_apps_common.h"

#define SIDE_X     8
#define SIDE_W     214
#define ITEM_H     52
#define LIST_Y     70
#define ED_Y       58
#define LH         21
#define MAXL       1600

typedef struct {
    int      file;
    int32_t  cursor;
    int32_t  scroll;
    int32_t  goal_x;
    int      blink_phase;
    int      hover_item;
    int      hover_btn;
    int32_t  caret_x, caret_y;
} notes_t;

static notes_t g_n[UI_MAX_WIN];
static int32_t g_starts[MAXL + 1];

static int32_t ed_x(void) { return SIDE_X + SIDE_W + 22; }
static int32_t ed_w(const ui_window_t *w) { return w->w - ed_x() - 26; }

static int note_at(int k)
{
    int f = -1, i = 0;

    while ((f = fs_child_next(fs_docs(), f)) >= 0) {
        fs_node_t *n = fs_node(f);
        if (n->type != FS_TEXT)
            continue;
        if (i == k)
            return f;
        i++;
    }
    return -1;
}

static int note_index(int file)
{
    int f = -1, i = 0;

    while ((f = fs_child_next(fs_docs(), f)) >= 0) {
        if (fs_node(f)->type != FS_TEXT)
            continue;
        if (f == file)
            return i;
        i++;
    }
    return -1;
}

static int layout(const char *t, int32_t maxw)
{
    static const char *cached_text;
    static int cached_width, cached_lines;
    static uint64_t cached_revision;
    int n = 0;
    int32_t i = 0, line_start = 0, last_space = -1, wsum = 0;

    if (t == cached_text && maxw == cached_width && fs_revision() == cached_revision)
        return cached_lines;
    cached_text = t;
    cached_width = maxw;
    cached_revision = fs_revision();
    g_starts[n++] = 0;
    while (t[i]) {
        int32_t j = i, cw;
        uint32_t cp;

        if (t[i] == '\n') {
            i++;
            if (n < MAXL)
                g_starts[n++] = i;
            line_start = i;
            last_space = -1;
            wsum = 0;
            continue;
        }
        cp = ui_utf8_next(t, &j);
        cw = ui_text_width_n(t + i, j - i, UI_FONT_UI);
        if (wsum + cw > maxw && i > line_start) {
            int32_t brk = (last_space >= line_start) ? last_space + 1 : i;
            if (n < MAXL)
                g_starts[n++] = brk;
            line_start = brk;
            last_space = -1;
            wsum = ui_text_width_n(t + brk, i - brk, UI_FONT_UI);
        }
        if (cp == ' ')
            last_space = i;
        wsum += cw;
        i = j;
    }
    g_starts[n] = i;
    cached_lines = n;
    return n;
}

static int32_t line_end(const char *t, int n, int l)
{
    int32_t e = g_starts[l + 1];

    if (l + 1 < n && e > g_starts[l] && t[e - 1] == '\n')
        e--;
    if (l + 1 >= n)
        e = (int32_t)strlen(t);
    return e;
}

static int line_of(int n, int32_t pos)
{
    int l = 0;

    while (l + 1 < n && g_starts[l + 1] <= pos)
        l++;
    return l;
}

static int lfont(int l)
{
    return l == 0 ? UI_FONT_UI_BOLD : UI_FONT_UI;
}

static int32_t pos_at_x(const char *t, int n, int l, int32_t x)
{
    int32_t i = g_starts[l], e = line_end(t, n, l), best = i, bx = 0;

    while (i < e) {
        int32_t j = i;
        int32_t cw;
        ui_utf8_next(t, &j);
        cw = ui_text_width_n(t + i, j - i, lfont(l));
        if (bx + cw / 2 >= x)
            return i;
        bx += cw;
        i = j;
        best = i;
    }
    return best;
}

static void ensure_visible(ui_window_t *w, notes_t *s)
{
    const char *t = fs_data(s->file);
    int n, l;
    int32_t eh = w->h - ED_Y - 16, cy;

    if (!t)
        return;
    n = layout(t, ed_w(w));
    l = line_of(n, s->cursor);
    cy = l * LH;
    if (cy < s->scroll)
        s->scroll = cy;
    if (cy + LH > s->scroll + eh)
        s->scroll = cy + LH - eh;
    if (s->scroll < 0)
        s->scroll = 0;
}

static void select_file(ui_window_t *w, notes_t *s, int file)
{
    s->file = file;
    s->cursor = 0;
    s->scroll = 0;
    s->goal_x = -1;
    if (fs_data(file))
        s->cursor = (int32_t)strlen(fs_data(file));
    ui_win_invalidate(w);
    ensure_visible(w, s);
}

static int new_note(ui_window_t *w, int slot)
{
    char name[48];
    int f;

    fs_unique_name(fs_docs(), "Nota nueva", ".txt", name, sizeof(name));
    f = fs_create(fs_docs(), name, FS_TEXT);
    if (f >= 0)
        select_file(w, &g_n[slot], f);
    return 1;
}

static void notes_open(ui_window_t *w, int slot, const char *arg)
{
    notes_t *s = &g_n[slot];
    int f = -1;

    if (arg) {
        f = fs_lookup(fs_root(), arg);
        if (!fs_node(f) || fs_node(f)->type != FS_TEXT)
            f = -1;
    }
    if (f < 0)
        f = note_at(0);
    if (f < 0)
        f = fs_create(fs_docs(), "Nota nueva.txt", FS_TEXT);
    s->hover_item = -1;
    s->hover_btn = -1;
    select_file(w, s, f);
    ui_strlcpy(w->title, "Notas", sizeof(w->title));
}

static void first_line(const char *t, char *out, int cap, int skip)
{
    int i = 0, n = 0;

    while (skip > 0 && t[i]) {
        if (t[i] == '\n')
            skip--;
        i++;
    }
    while (t[i] == '\n')
        i++;
    while (t[i] && t[i] != '\n' && n < cap - 1)
        out[n++] = t[i++];
    out[n] = '\0';
}

static void notes_draw(ui_window_t *w, int slot, ui_canvas_t *c, int32_t ox, int32_t oy, int focused)
{
    notes_t *s = &g_n[slot];
    const char *t = fs_data(s->file);
    int k, f, n, l;
    int32_t ex = ox + ed_x(), ew = ed_w(w), ey = oy + ED_Y;
    int32_t cy0 = c->cy0, cy1 = c->cy1;
    char name[48], prev[64];

    /* lista */
    ui_text(c, ox + SIDE_X + 16, oy + 44, "Documentos", UI_FONT_SMALL, UI_TEXT_2, 255u);
    for (k = 0; (f = note_at(k)) >= 0; k++) {
        int32_t iy = oy + LIST_Y + k * ITEM_H;
        fs_node_t *nd = fs_node(f);
        int32_t nl;

        if (iy > oy + w->h - 20)
            break;
        if (f == s->file)
            ui_rrect(c, (float)(ox + SIDE_X + 8), (float)iy, (float)(SIDE_W - 16), (float)(ITEM_H - 4),
                     5.0f, 0xBBDCD1u, focused ? 240u : 150u);
        else if (k == s->hover_item)
            ui_rrect(c, (float)(ox + SIDE_X + 8), (float)iy, (float)(SIDE_W - 16), (float)(ITEM_H - 4),
                     9.0f, 0x000000u, 14u);
        ui_strlcpy(name, nd->name, sizeof(name));
        nl = (int32_t)strlen(name);
        if (nl > 4 && ui_streq(name + nl - 4, ".txt"))
            name[nl - 4] = '\0';
        ui_text_fit(c, ox + SIDE_X + 18, iy + 6, name, UI_FONT_UI_BOLD, UI_TEXT, 255u, SIDE_W - 36);
        first_line(fs_data(f) ? fs_data(f) : "", prev, sizeof(prev), 1);
        ui_text_fit(c, ox + SIDE_X + 18, iy + 26, prev[0] ? prev : "Sin texto adicional",
                    UI_FONT_SMALL, UI_TEXT_2, 255u, SIDE_W - 36);
    }

    /* barra de herramientas */
    if (fs_node(s->file)) {
        int32_t nl;
        ui_strlcpy(name, fs_node(s->file)->name, sizeof(name));
        nl = (int32_t)strlen(name);
        if (nl > 4 && ui_streq(name + nl - 4, ".txt"))
            name[nl - 4] = '\0';
        ui_text_fit(c, ex, oy + 15, name, UI_FONT_UI_BOLD, UI_TEXT, 255u, ew - 110);
    }
    app_pill(c, ox + w->w - 96, oy + 10, 36, 30, s->hover_btn == 1, 0);
    app_trash_glyph(c, (float)(ox + w->w - 78), (float)(oy + 26), 0x3A3A3Cu, 255u);
    app_pill(c, ox + w->w - 52, oy + 10, 36, 30, s->hover_btn == 0, 0);
    app_compose_glyph(c, (float)(ox + w->w - 33), (float)(oy + 25), 0x3A3A3Cu, 255u);

    if (!t)
        return;

    /* editor */
    ui_fill(c, ex, oy + 45, ew, 1, UI_SEPARATOR, 255);
    c->cy0 = ui_max(cy0, ey);
    c->cy1 = ui_min(cy1, oy + w->h - 10);
    n = layout(t, ew);
    for (l = 0; l < n; l++) {
        int32_t ly = ey + l * LH - s->scroll;
        if (ly + LH < c->cy0)
            continue;
        if (ly > c->cy1)
            break;
        ui_text_n(c, ex, ly, t + g_starts[l], line_end(t, n, l) - g_starts[l], lfont(l),
                  UI_TEXT, 255u);
    }
    if (t[0] == '\0')
        ui_text(c, ex, ey, "Escribe aquí…", UI_FONT_UI, 0xAEAEB2u, 255u);

    if (focused) {
        int32_t cl = line_of(n, s->cursor);
        int32_t cx = ex + ui_text_width_n(t + g_starts[cl], s->cursor - g_starts[cl], lfont(cl));
        int32_t cyy = ey + cl * LH - s->scroll;
        s->caret_x = cx-ox;
        s->caret_y = cyy-oy+1;
        if (!s->blink_phase)
            ui_fill(c, cx, cyy + 1, 2, LH - 2, UI_ACCENT, 255u);
    }
    c->cy0 = cy0;
    c->cy1 = cy1;
}

static void insert_text(notes_t *s, const char *txt)
{
    char *d = fs_data(s->file);
    int32_t len, n = (int32_t)strlen(txt);

    if (!d)
        return;
    len = (int32_t)strlen(d);
    if (len + n >= FS_FILE_CAP - 1)
        return;
    ui_memmove(d + s->cursor + n, d + s->cursor, len - s->cursor + 1);
    memcpy(d + s->cursor, txt, (size_t)n);
    s->cursor += n;
    fs_node(s->file)->size = len + n;
    fs_changed(s->file);
}

static void delete_range(notes_t *s, int32_t a, int32_t b)
{
    char *d = fs_data(s->file);
    int32_t len;

    if (!d || a >= b)
        return;
    len = (int32_t)strlen(d);
    ui_memmove(d + a, d + b, len - b + 1);
    s->cursor = a;
    fs_node(s->file)->size = len - (b - a);
    fs_changed(s->file);
}

static void notes_key(ui_window_t *w, int slot, const input_event_t *ev, const char *text)
{
    notes_t *s = &g_n[slot];
    const char *t = fs_data(s->file);
    int n, l;
    int32_t len;

    if (!t)
        return;
    len = (int32_t)strlen(t);
    n = layout(t, ed_w(w));
    l = line_of(n, s->cursor);

    switch (ev->u.key.keycode) {
    case INPUT_KEY_LEFT:
        s->cursor = ui_utf8_prev(t, s->cursor);
        s->goal_x = -1;
        break;
    case INPUT_KEY_RIGHT:
        if (s->cursor < len) {
            int32_t j = s->cursor;
            ui_utf8_next(t, &j);
            s->cursor = j;
        }
        s->goal_x = -1;
        break;
    case INPUT_KEY_UP:
    case INPUT_KEY_DOWN: {
        int nl = l + (ev->u.key.keycode == INPUT_KEY_UP ? -1 : 1);
        if (s->goal_x < 0)
            s->goal_x = ui_text_width_n(t + g_starts[l], s->cursor - g_starts[l], lfont(l));
        if (nl < 0)
            s->cursor = 0;
        else if (nl >= n)
            s->cursor = len;
        else
            s->cursor = pos_at_x(t, n, nl, s->goal_x);
        break;
    }
    case INPUT_KEY_HOME:
        s->cursor = g_starts[l];
        break;
    case INPUT_KEY_END:
        s->cursor = line_end(t, n, l);
        break;
    case INPUT_KEY_PAGEUP:
        s->scroll = ui_max(0, s->scroll - (w->h - ED_Y - 40));
        ui_win_invalidate(w);
        return;
    case INPUT_KEY_PAGEDOWN:
        s->scroll += w->h - ED_Y - 40;
        ui_win_invalidate(w);
        return;
    case INPUT_KEY_BACKSPACE:
        if (s->cursor > 0)
            delete_range(s, ui_utf8_prev(t, s->cursor), s->cursor);
        s->goal_x = -1;
        break;
    case INPUT_KEY_DELETE:
        if (s->cursor < len) {
            int32_t j = s->cursor;
            ui_utf8_next(t, &j);
            delete_range(s, s->cursor, j);
        }
        break;
    case INPUT_KEY_ENTER:
        insert_text(s, "\n");
        s->goal_x = -1;
        break;
    case INPUT_KEY_TAB:
        insert_text(s, "    ");
        break;
    case INPUT_KEY_ESC:
        return;
    default:
        if (!text[0])
            return;
        insert_text(s, text);
        s->goal_x = -1;
        break;
    }
    s->blink_phase = 0;
    ensure_visible(w, s);
    ui_win_invalidate(w);
}

static void notes_mouse(ui_window_t *w, int slot, int type, int32_t lx, int32_t ly, int btn, int clicks)
{
    notes_t *s = &g_n[slot];
    int hb = -1, hi = -1;
    (void)clicks;

    if (type == UI_M_LEAVE) {
        if (s->hover_btn != -1 || s->hover_item != -1) {
            s->hover_btn = -1;
            s->hover_item = -1;
            ui_win_invalidate(w);
        }
        return;
    }

    if (app_hit(lx, ly, w->w - 52, 10, 36, 30))
        hb = 0;
    else if (app_hit(lx, ly, w->w - 96, 10, 36, 30))
        hb = 1;
    if (lx >= SIDE_X && lx < SIDE_X + SIDE_W && ly >= LIST_Y) {
        int k = (ly - LIST_Y) / ITEM_H;
        if (note_at(k) >= 0)
            hi = k;
    }

    if (type == UI_M_HOVER) {
        if (hb != s->hover_btn || hi != s->hover_item) {
            s->hover_btn = hb;
            s->hover_item = hi;
            ui_win_invalidate(w);
        }
        return;
    }

    if (type != UI_M_DOWN || btn != INPUT_BTN_LEFT) {
        if (type == UI_M_DOWN && btn == INPUT_BTN_RIGHT)
            ui_consume();
        return;
    }

    if (hb == 0) {
        ui_consume();
        new_note(w, slot);
        return;
    }
    if (hb == 1) {
        int idx = note_index(s->file), nf;
        ui_consume();
        fs_move(s->file, fs_trash());
        nf = note_at(idx > 0 ? idx - 1 : 0);
        if (nf < 0)
            nf = fs_create(fs_docs(), "Nota nueva.txt", FS_TEXT);
        select_file(w, s, nf);
        return;
    }
    if (hi >= 0) {
        ui_consume();
        select_file(w, s, note_at(hi));
        return;
    }
    if (lx >= ed_x() - 10 && ly >= ED_Y - 6) {
        const char *t = fs_data(s->file);
        int n, l;

        ui_consume();
        if (!t)
            return;
        n = layout(t, ed_w(w));
        l = (ly - ED_Y + s->scroll) / LH;
        if (l < 0)
            l = 0;
        if (l >= n) {
            s->cursor = (int32_t)strlen(t);
        } else {
            s->cursor = pos_at_x(t, n, l, lx - ed_x());
        }
        s->goal_x = -1;
        s->blink_phase = 0;
        ui_win_invalidate(w);
    }
}

static void notes_wheel(ui_window_t *w, int slot, int delta)
{
    notes_t *s = &g_n[slot];

    s->scroll += delta * LH * 3;
    if (s->scroll < 0)
        s->scroll = 0;
    ui_win_invalidate(w);
}

static int notes_tick(ui_window_t *w, int slot, uint64_t now)
{
    notes_t *s = &g_n[slot];
    int ph = (int)((now / 530u) % 2u);

    if (!ui_win_focused(w))
        return 0;
    if (ph != s->blink_phase) {
        s->blink_phase = ph;
        if (s->caret_y >= ED_Y && s->caret_y < w->h - 10)
            ui_win_invalidate_local(w, s->caret_x, s->caret_y, 2, LH-2);
        return 1;
    }
    return 0;
}

static int notes_glass(ui_window_t *w, int slot, ui_rect_t *out, int max)
{
    (void)slot;
    if (max < 1)
        return 0;
    out[0] = (ui_rect_t){ SIDE_X, 8, SIDE_W, w->h - 16 };
    return 1;
}

const ui_app_t app_notes = {
    "Notas", UI_ICON_NOTES, 820, 540, 520, 320, 0xFFFFFFu, 0, 0, 0, 1,
    notes_open, notes_draw, notes_mouse, notes_key, notes_wheel, notes_tick, notes_glass, new_note,
};

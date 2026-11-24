/*
 * MettaOS shell — escritorio estilo Tahoe: barra de menús, dock Liquid Glass con
 * magnificación, gestor de ventanas, menús, búsqueda, calendario y teclado.
 */
#include "ui_app.h"
#include <network.h>

#include <interrupt/pit.h>
#include <interrupt/ports.h>

#define MENUBAR_H   38
#define WIN_RADIUS  8.0f
#define SHADOW_PAD  34

const ui_glass_t UI_GLASS_LIGHT = { 7, 0xF4F8FFu, 70u, 1.45f, 9.0f, 1.0f, 0 };
const ui_glass_t UI_GLASS_MENU  = { 14, 0xF7F9FCu, 150u, 1.30f, 6.0f, 0.9f, 0 };
const ui_glass_t UI_GLASS_DARK  = { 12, 0x1A1B20u, 205u, 1.35f, 8.0f, 0.55f, 1 };

/* ------------------------------------------------------------------ estado */

static ui_canvas_t g_cv;
static int32_t     W, H;
static uint32_t    g_wall[1024 * 768];
static int         g_wall_idx;
static int         g_glass_enabled;

int ui_liquid_glass(void) { return g_glass_enabled; }

int ui_set_liquid_glass(int enabled)
{
    g_glass_enabled = !!enabled;
    ui_invalidate(0, 0, W, H);
    return fs_write_glass(g_glass_enabled);
}

static void panel_material(ui_canvas_t *c, int x, int y, int w, int h, float r,
                           const ui_glass_t *glass, uint32_t solid)
{
    if (g_glass_enabled) ui_glass(c, x, y, w, h, r, glass);
    else ui_rrect(c, x, y, w, h, r, solid, 255);
}
static int         g_bar_light_text;

static ui_window_t g_win[UI_MAX_WIN];
static int         g_order[UI_MAX_WIN]; /* abajo → arriba */
static int         g_order_n;
static int         g_focus = -1;
static int         g_next_id = 1;

static ui_rect_t   g_dirty;
static uint64_t    g_now;
static int32_t     g_mx, g_my;
static int         g_btn_left;

enum { DRAG_NONE = 0, DRAG_MOVE, DRAG_RESIZE, DRAG_APP, DRAG_LIGHT };
static int        g_drag;
static int        g_drag_win = -1;
static int32_t    g_drag_mx, g_drag_my;
static ui_rect_t  g_drag_start;
static int        g_resize_edges;
static int        g_light_pressed;
static int        g_consumed;

static uint64_t   g_last_click_ms;
static int32_t    g_last_click_x, g_last_click_y;
static int        g_click_count;

static int        g_hover_win = -1;
static int        g_lights_hover = -1;

static int        g_kbd_layout;
static int        g_caps;
static int        g_altgr;
static uint32_t   g_dead;

static ui_time_t  g_time;
static uint64_t   g_last_rtc;

/* dock */
typedef struct {
    const char *name;
    int         icon;
    int         app; /* -1 = Papelera */
} dock_item_t;

static const dock_item_t k_dock[] = {
    { "Archivos", UI_ICON_FILES, APP_FILES },
    { "Notas", UI_ICON_NOTES, APP_NOTES },
    { "Terminal", UI_ICON_TERMINAL, APP_TERMINAL },
    { "Calculadora", UI_ICON_CALC, APP_CALC },
    { "Ajustes", UI_ICON_SETTINGS, APP_SETTINGS },
    { "Papelera", UI_ICON_TRASH, -1 },
};
#define DOCK_N        6
#define DOCK_SEP      5
#define DOCK_S        44.0f
#define DOCK_PAD      18.0f
#define DOCK_GAP      29.0f
#define DOCK_BOTTOM   14.0f
#define DOCK_MAG      0.12f
#define DOCK_ZONE_H   200

static float      g_dock_scale[DOCK_N];
static int        g_dock_hover = -1;
static uint64_t   g_dock_bounce[DOCK_N];

/* menús */
#define MENU_MAX 24
static struct {
    int            open;
    int32_t        x, y, w, h;
    ui_menu_item_t items[MENU_MAX];
    int            n;
    int            hover;
    ui_menu_cb     cb;
    void          *ctx;
    int            bar_item;
} g_menu;

enum { BAR_LOGO = 0, BAR_APP, BAR_FILE, BAR_WINDOW, BAR_HELP, BAR_ITEMS };
static ui_rect_t  g_bar_rects[BAR_ITEMS];
static ui_rect_t  g_bar_clock, g_bar_search;
static int        g_bar_hover = -1;

/* búsqueda */
#define SPOT_MAX 7
static struct {
    int  open;
    char q[64];
    int  sel;
    int  kind[SPOT_MAX]; /* 0 app, 1 archivo */
    int  ref[SPOT_MAX];
    int  n;
} g_spot;

static int        g_desk_sel = -1;

static void desk_item_menu_cb(int id, void *ctx);

/* ---------------------------------------------------------------- helpers */

uint64_t ui_now(void) { return g_now; }
int32_t  ui_screen_w(void) { return W; }
int32_t  ui_screen_h(void) { return H; }
void     ui_consume(void) { g_consumed = 1; }
int      ui_kbd_layout_get(void) { return g_kbd_layout; }
void     ui_kbd_layout_set(int idx) { g_kbd_layout = idx ? 1 : 0; }
int      ui_wallpaper_get(void) { return g_wall_idx; }

void ui_invalidate(int32_t x, int32_t y, int32_t w, int32_t h)
{
    ui_rect_t r = { x, y, w, h };

    if (r.x < 0) {
        r.w += r.x;
        r.x = 0;
    }
    if (r.y < 0) {
        r.h += r.y;
        r.y = 0;
    }
    if (r.x + r.w > W)
        r.w = W - r.x;
    if (r.y + r.h > H)
        r.h = H - r.y;
    ui_rect_union(&g_dirty, &r);
}

static void invalidate_win_full(const ui_window_t *w)
{
    ui_invalidate(w->x - SHADOW_PAD, w->y - SHADOW_PAD, w->w + SHADOW_PAD * 2,
                  w->h + SHADOW_PAD * 2);
}

void ui_win_invalidate(const ui_window_t *w)
{
    ui_invalidate(w->x, w->y, w->w, w->h);
}

void ui_win_invalidate_local(const ui_window_t *w, int32_t x, int32_t y, int32_t ww, int32_t hh)
{
    ui_invalidate(w->x + x, w->y + y, ww, hh);
}

int ui_win_slot(const ui_window_t *w)
{
    return (int)(w - g_win);
}

ui_window_t *ui_window_by_slot(int slot)
{
    if (slot < 0 || slot >= UI_MAX_WIN || !g_win[slot].used)
        return NULL;
    return &g_win[slot];
}

int ui_win_focused(const ui_window_t *w)
{
    return g_focus >= 0 && &g_win[g_focus] == w;
}

static void invalidate_dock(void)
{
    ui_invalidate(0, H - DOCK_ZONE_H, W, DOCK_ZONE_H);
}

static void invalidate_bar(void)
{
    ui_invalidate(0, 0, W, MENUBAR_H);
}

static ui_rect_t work_area(void)
{
    ui_rect_t r = { 8, MENUBAR_H + 6, W - 16, H - MENUBAR_H - 6 - (int32_t)(DOCK_S + DOCK_PAD * 2 + DOCK_BOTTOM) - 10 };
    return r;
}

void ui_reboot(void)
{
    fs_sync();
    uint32_t i;

    for (i = 0; i < 100000u && (inb(0x64) & 0x02u); i++)
        ;
    outb(0x64, 0xFE);
    for (;;)
        __asm__ volatile("hlt");
}

void ui_shutdown(void)
{
    fs_sync();
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    for (;;)
        __asm__ volatile("cli; hlt");
}

/* --------------------------------------------------------------- wallpaper */

void ui_wallpaper_set(int idx)
{
    uint64_t lum = 0;
    int32_t x, y;

    g_wall_idx = ui_clampi(idx, 0, UI_WALL_COUNT - 1);
    ui_wall_render(g_wall_idx, g_wall, W, H, W, W, H);
    for (y = 0; y < MENUBAR_H; y += 3) {
        for (x = 0; x < W; x += 8) {
            uint32_t p = g_wall[y * W + x];
            lum += (((p >> 16) & 0xFFu) * 77u + ((p >> 8) & 0xFFu) * 150u + (p & 0xFFu) * 29u) >> 8;
        }
    }
    lum /= (uint64_t)((MENUBAR_H / 3 + 1) * (W / 8));
    g_bar_light_text = lum < 140u;
    ui_invalidate(0, 0, W, H);
}

/* ----------------------------------------------------------------- ventanas */

static int win_index_in_order(int wi)
{
    int i;

    for (i = 0; i < g_order_n; i++) {
        if (g_order[i] == wi)
            return i;
    }
    return -1;
}

static void focus_window(int wi)
{
    int pos, i;

    if (g_focus == wi && wi >= 0 && g_order_n > 0 && g_order[g_order_n - 1] == wi)
        return;
    if (g_focus >= 0 && g_win[g_focus].app == APP_LINUX)
        ui_linux_release(g_focus);
    if (g_focus >= 0 && g_win[g_focus].used)
        invalidate_win_full(&g_win[g_focus]);
    if (wi >= 0) {
        pos = win_index_in_order(wi);
        if (pos >= 0) {
            for (i = pos; i < g_order_n - 1; i++)
                g_order[i] = g_order[i + 1];
            g_order[g_order_n - 1] = wi;
        }
        invalidate_win_full(&g_win[wi]);
    }
    g_focus = wi;
    invalidate_bar();
}

static void focus_topmost_visible(void)
{
    int i;

    for (i = g_order_n - 1; i >= 0; i--) {
        if (!g_win[g_order[i]].minimized) {
            focus_window(g_order[i]);
            return;
        }
    }
    g_focus = -1;
    invalidate_bar();
}

static int app_window_count(int app)
{
    int i, n = 0;

    for (i = 0; i < UI_MAX_WIN; i++) {
        if (g_win[i].used && g_win[i].app == app)
            n++;
    }
    return n;
}

ui_window_t *ui_open_app(int app, const char *arg)
{
    const ui_app_t *a;
    ui_window_t *w;
    int i, wi = -1, n;
    ui_rect_t wa;

    if (app < 0 || app >= APP_COUNT)
        return NULL;
    a = ui_apps[app];

    if (a->single) {
        for (i = g_order_n - 1; i >= 0; i--) {
            if (g_win[g_order[i]].app == app) {
                w = &g_win[g_order[i]];
                if (w->minimized) {
                    w->minimized = 0;
                }
                if (arg)
                    a->open(w, g_order[i], arg);
                focus_window(g_order[i]);
                invalidate_win_full(w);
                return w;
            }
        }
    }

    for (i = 0; i < UI_MAX_WIN; i++) {
        if (!g_win[i].used) {
            wi = i;
            break;
        }
    }
    if (wi < 0)
        return NULL;

    w = &g_win[wi];
    memset(w, 0, sizeof(*w));
    w->used = 1;
    w->id = g_next_id++;
    w->app = app;
    ui_strlcpy(w->title, a->name, sizeof(w->title));
    wa = work_area();
    w->w = ui_min(a->def_w, wa.w);
    w->h = ui_min(a->def_h, wa.h);
    n = app_window_count(app) - 1 + g_order_n;
    w->x = wa.x + (wa.w - w->w) / 2 + (n % 6) * 26 - 60;
    w->y = wa.y + (wa.h - w->h) / 2 + (n % 6) * 22 - 40;
    w->x = ui_clampi(w->x, wa.x, wa.x + wa.w - w->w);
    w->y = ui_clampi(w->y, wa.y, wa.y + wa.h - w->h);

    g_order[g_order_n++] = wi;
    a->open(w, wi, arg);
    focus_window(wi);
    invalidate_win_full(w);
    invalidate_dock();

    for (i = 0; i < DOCK_N; i++) {
        if (k_dock[i].app == app)
            g_dock_bounce[i] = g_now;
    }
    return w;
}

void ui_close_window(ui_window_t *w)
{
    int wi = ui_win_slot(w), pos, i;

    if (!w->used)
        return;
    if (w->app == APP_LINUX)
        ui_linux_close(wi);
    invalidate_win_full(w);
    w->used = 0;
    pos = win_index_in_order(wi);
    if (pos >= 0) {
        for (i = pos; i < g_order_n - 1; i++)
            g_order[i] = g_order[i + 1];
        g_order_n--;
    }
    if (g_drag_win == wi) {
        g_drag = DRAG_NONE;
        g_drag_win = -1;
    }
    if (g_hover_win == wi)
        g_hover_win = -1;
    if (g_focus == wi) {
        g_focus = -1;
        focus_topmost_visible();
    }
    invalidate_dock();
    invalidate_bar();
}

static void minimize_window(ui_window_t *w)
{
    invalidate_win_full(w);
    w->minimized = 1;
    if (g_focus == ui_win_slot(w)) {
        g_focus = -1;
        focus_topmost_visible();
    }
    invalidate_dock();
}

static void toggle_zoom(ui_window_t *w)
{
    ui_rect_t wa = work_area();

    invalidate_win_full(w);
    if (w->maximized) {
        w->x = w->rx;
        w->y = w->ry;
        w->w = w->rw;
        w->h = w->rh;
        w->maximized = 0;
    } else {
        w->rx = w->x;
        w->ry = w->y;
        w->rw = w->w;
        w->rh = w->h;
        w->x = wa.x;
        w->y = wa.y;
        w->w = wa.w;
        w->h = wa.h;
        w->maximized = 1;
    }
    invalidate_win_full(w);
}

static void close_app(int app)
{
    int i;

    for (i = 0; i < UI_MAX_WIN; i++) {
        if (g_win[i].used && g_win[i].app == app)
            ui_close_window(&g_win[i]);
    }
}

static void cycle_focus(void)
{
    int i;

    if (g_order_n <= 0)
        return;
    for (i = 0; i < g_order_n; i++) {
        int wi = g_order[0];
        int k;

        for (k = 0; k < g_order_n - 1; k++)
            g_order[k] = g_order[k + 1];
        g_order[g_order_n - 1] = wi;
        if (!g_win[wi].minimized) {
            g_focus = -1;
            focus_window(wi);
            ui_invalidate(0, 0, W, H);
            return;
        }
    }
}

static int window_at(int32_t x, int32_t y, int margin)
{
    int i;

    for (i = g_order_n - 1; i >= 0; i--) {
        ui_window_t *w = &g_win[g_order[i]];
        ui_rect_t r = { w->x - margin, w->y - margin, w->w + margin * 2, w->h + margin * 2 };

        if (!w->minimized && ui_rect_has(&r, x, y))
            return g_order[i];
    }
    return -1;
}

static ui_rect_t light_rect(const ui_window_t *w, int k)
{
    ui_rect_t r = { w->x + 14 + k * 20, w->y + 16, 14, 14 };
    return r;
}

static int lights_hit(const ui_window_t *w, int32_t x, int32_t y)
{
    int k;

    for (k = 0; k < 3; k++) {
        ui_rect_t r = light_rect(w, k);
        r.x -= 3;
        r.y -= 3;
        r.w += 6;
        r.h += 6;
        if (ui_rect_has(&r, x, y))
            return k;
    }
    return -1;
}

static int lights_group_hit(const ui_window_t *w, int32_t x, int32_t y)
{
    ui_rect_t r = { w->x + 8, w->y + 10, 70, 26 };
    return ui_rect_has(&r, x, y);
}

static int resize_edges_at(const ui_window_t *w, int32_t x, int32_t y)
{
    int e = 0;
    const int m = 6;

    if (w->maximized)
        return 0;
    if (y > w->y + 20) {
        if (x < w->x + m)
            e |= 1;
        if (x >= w->x + w->w - m)
            e |= 2;
    }
    if (y >= w->y + w->h - m)
        e |= 4;
    if (x < w->x - 1 || x > w->x + w->w || y > w->y + w->h)
        e |= (x < w->x ? 1 : 0) | (x >= w->x + w->w ? 2 : 0) | (y >= w->y + w->h ? 4 : 0);
    return e;
}

/* -------------------------------------------------------------------- dock */

static void dock_geom(float *ix, float *iy, float *isz, ui_rect_t *panel)
{
    float total = 0.0f, x, ph;
    int i;
    uint64_t t;

    for (i = 0; i < DOCK_N; i++)
        total += DOCK_S * g_dock_scale[i];
    total += DOCK_GAP * (float)(DOCK_N - 1) + 13.0f;
    ph = DOCK_S + DOCK_PAD * 2.0f;
    panel->w = (int32_t)(total + DOCK_PAD * 2.0f + 0.5f);
    panel->h = (int32_t)ph;
    panel->x = (W - panel->w) / 2;
    panel->y = H - (int32_t)DOCK_BOTTOM - panel->h;

    x = (float)panel->x + DOCK_PAD;
    for (i = 0; i < DOCK_N; i++) {
        float s = DOCK_S * g_dock_scale[i];
        float bounce = 0.0f;

        if (i == DOCK_SEP)
            x += 13.0f;
        t = g_now - g_dock_bounce[i];
        if (g_dock_bounce[i] && t < 700u) {
            float ph2 = (float)t / 700.0f;
            bounce = ui_absf(ui_sinf(ph2 * 3.14159f * 2.0f)) * 18.0f * (1.0f - ph2);
        }
        ix[i] = x;
        isz[i] = s;
        iy[i] = (float)panel->y + 8.0f + DOCK_S - s - bounce;
        x += s + DOCK_GAP;
    }
}

static int dock_hit(int32_t mx, int32_t my)
{
    float ix[DOCK_N], iy[DOCK_N], isz[DOCK_N];
    ui_rect_t p;
    int i;

    dock_geom(ix, iy, isz, &p);
    if (my < p.y - 30 || mx < p.x || mx >= p.x + p.w)
        return -2;
    for (i = 0; i < DOCK_N; i++) {
        if ((float)mx >= ix[i] - DOCK_GAP * 0.5f && (float)mx < ix[i] + isz[i] + DOCK_GAP * 0.5f &&
            (float)my >= iy[i] && my < p.y + p.h)
            return i;
    }
    return (my >= p.y) ? -1 : -2;
}

static void dock_update_targets(int force)
{
    ui_rect_t p;
    float ix[DOCK_N], iy[DOCK_N], isz[DOCK_N];
    float base_x;
    int i, inside, moving = 0;

    dock_geom(ix, iy, isz, &p);
    inside = (g_mx >= p.x - 10 && g_mx < p.x + p.w + 10 && g_my >= p.y - 12 && g_my < H) &&
             g_drag == DRAG_NONE && !g_menu.open;

    base_x = (float)(W - (int32_t)(DOCK_S * DOCK_N + DOCK_GAP * (DOCK_N - 1) + 13.0f + DOCK_PAD * 2.0f)) * 0.5f + DOCK_PAD;
    for (i = 0; i < DOCK_N; i++) {
        float target = 1.0f, cur = g_dock_scale[i];
        float cx = base_x + (float)i * (DOCK_S + DOCK_GAP) + (i >= DOCK_SEP ? 13.0f : 0.0f) + DOCK_S * 0.5f;

        if (inside) {
            float d = ((float)g_mx - cx) / (DOCK_S * 2.2f);
            float f = 1.0f - d * d;
            if (f > 0.0f)
                target = 1.0f + DOCK_MAG * f * f;
        }
        cur += (target - cur) * 0.35f;
        if (ui_absf(target - cur) < 0.004f)
            cur = target;
        if (cur != g_dock_scale[i])
            moving = 1;
        g_dock_scale[i] = cur;
        if (g_dock_bounce[i] && g_now - g_dock_bounce[i] < 760u)
            moving = 1;
    }
    if (moving || force)
        invalidate_dock();
}

static int dock_app_running(int app)
{
    return app >= 0 && app_window_count(app) > 0;
}

static int dock_target(int i)
{
    int app = k_dock[i].app;
    return app == APP_TERMINAL && ui_linux_attached() ? APP_LINUX : app;
}

static void dock_click(int i)
{
    int app = dock_target(i), k;

    if (app < 0) {
        char path[160];
        fs_path(fs_trash(), path, sizeof(path));
        ui_open_app(APP_FILES, path);
        return;
    }
    if (dock_app_running(app)) {
        for (k = g_order_n - 1; k >= 0; k--) {
            ui_window_t *w = &g_win[g_order[k]];
            if (w->app == app) {
                if (w->minimized) {
                    w->minimized = 0;
                    invalidate_win_full(w);
                }
                focus_window(g_order[k]);
                return;
            }
        }
    }
    ui_open_app(app, NULL);
}

static void draw_dock(ui_canvas_t *c)
{
    float ix[DOCK_N], iy[DOCK_N], isz[DOCK_N];
    ui_rect_t p;
    int i;
    float r;

    dock_geom(ix, iy, isz, &p);
    r = 12.0f;
    ui_shadow(c, (float)p.x, (float)p.y, (float)p.w, (float)p.h, r, 16.0f, 5.0f, 64u);
    panel_material(c, p.x, p.y, p.w, p.h, r, &UI_GLASS_LIGHT, 0xF4F8F6);
    ui_rrect_stroke(c, p.x, p.y, p.w, p.h, r, 1, 0xFFFFFF, 235);

    {
        float sx = ix[DOCK_SEP] - 21.0f;
        ui_capsule(c, sx, (float)p.y + 12.0f, sx, (float)(p.y + p.h) - 12.0f, 1.2f,
                   0x000000u, 50u);
    }

    for (i = 0; i < DOCK_N; i++) {
        float cx = ix[i] + isz[i]*0.5f;
        if (i == g_dock_hover)
            ui_rrect(c, cx - 33, p.y + 4, 66, p.h - 8, 7, 0xD6E5E1, 255);
        ui_icon_draw(c, k_dock[i].icon, ix[i], iy[i], isz[i]);
        ui_text_center(c, (int32_t)cx, p.y + p.h - 25, k_dock[i].name, UI_FONT_SMALL, UI_TEXT, 255);
        if (dock_app_running(dock_target(i)))
            ui_rrect(c, cx-6, p.y+p.h-6, 12, 2, 1, UI_ACCENT, 255);
    }
}

/* ---------------------------------------------------------------- menubar */

static const char *k_wday[7] = { "dom", "lun", "mar", "mié", "jue", "vie", "sáb" };
static const char *k_mon[12] = { "ene", "feb", "mar", "abr", "may", "jun",
                                 "jul", "ago", "sep", "oct", "nov", "dic" };
static void clock_text(char *out, size_t cap);

static const char *focused_app_name(void)
{
    if (g_focus >= 0 && g_win[g_focus].used)
        return ui_apps[g_win[g_focus].app]->name;
    return "Escritorio";
}

static void bar_layout(void)
{
    static const char *labels[BAR_ITEMS] = { "", "", "Archivo", "Ventana", "Ayuda" };
    int32_t x = 10;
    int i;
    char clock[48];

    for (i = 0; i < BAR_ITEMS; i++) {
        int32_t tw;

        if (i == BAR_LOGO)
            tw = 82;
        else if (i == BAR_APP)
            tw = ui_text_width(focused_app_name(), UI_FONT_UI_BOLD);
        else
            tw = ui_text_width(labels[i], UI_FONT_UI);
        g_bar_rects[i].x = x;
        g_bar_rects[i].y = 3;
        g_bar_rects[i].w = tw + 16;
        g_bar_rects[i].h = MENUBAR_H - 6;
        x += tw + 16 + 2;
    }

    clock_text(clock, sizeof(clock));
    g_bar_clock.w = ui_text_width(clock, UI_FONT_UI) + 16;
    g_bar_clock.x = W - 8 - g_bar_clock.w;
    g_bar_clock.y = 3;
    g_bar_clock.h = MENUBAR_H - 6;
    g_bar_search.w = 81;
    g_bar_search.x = g_bar_clock.x - g_bar_search.w - 46;
    g_bar_search.y = 3;
    g_bar_search.h = MENUBAR_H - 6;
}

static void clock_text(char *out, size_t cap)
{
    char tmp[8];

    out[0] = '\0';
    ui_strlcat(out, k_wday[g_time.wday % 7], cap);
    ui_strlcat(out, " ", cap);
    ui_itoa(g_time.day, tmp);
    ui_strlcat(out, tmp, cap);
    ui_strlcat(out, " ", cap);
    ui_strlcat(out, k_mon[(g_time.month + 11) % 12], cap);
    ui_strlcat(out, "  ", cap);
    ui_itoa(g_time.hour, tmp);
    ui_strlcat(out, tmp, cap);
    ui_strlcat(out, ":", cap);
    ui_fmt2(tmp, g_time.minute);
    ui_strlcat(out, tmp, cap);
}

static void bar_pill(ui_canvas_t *c, const ui_rect_t *r)
{
    ui_rrect(c, (float)r->x, (float)r->y, (float)r->w, (float)r->h, 7.0f,
             UI_ACCENT, 28u);
}

static void draw_menubar(ui_canvas_t *c)
{
    static const char *labels[BAR_ITEMS] = { "", "", "Archivo", "Ventana", "Ayuda" };
    int i;
    char clock[48];
    int32_t ty = (MENUBAR_H - ui_font_line(UI_FONT_UI)) / 2;
    uint32_t fg = UI_TEXT;
    float bx, by;

    bar_layout();
    panel_material(c, 0, 0, W, MENUBAR_H, 0, &UI_GLASS_MENU, 0xF5F8F6);
    ui_fill(c, 0, MENUBAR_H-1, W, 1, UI_SEPARATOR, 255);

    for (i = 0; i < BAR_ITEMS; i++) {
        const ui_rect_t *r = &g_bar_rects[i];

        if (g_menu.open && g_menu.bar_item == i)
            bar_pill(c, r);
        if (i == BAR_LOGO) {
            ui_logo_draw(c, r->x+13, MENUBAR_H*0.5f, 24, UI_ACCENT, 255);
            ui_text(c, r->x+32, ty, "MettaOS", UI_FONT_UI_BOLD, UI_TEXT, 255);
            ui_fill(c, r->x+r->w-1, 10, 1, MENUBAR_H-20, UI_SEPARATOR, 255);
        }
        else if (i == BAR_APP)
            ui_text(c, r->x + 8, ty, focused_app_name(), UI_FONT_UI_BOLD, fg, 255u);
        else
            ui_text(c, r->x + 8, ty, labels[i], UI_FONT_UI, fg, 255u);
    }

    /* derecha: búsqueda, wifi, batería, reloj */
    ui_rrect(c, g_bar_search.x, 6, g_bar_search.w, MENUBAR_H-12, 6,
             g_spot.open ? 0xCDDFD9 : 0xE3ECE8, 255);
    ui_text(c, g_bar_search.x+28, ty, "Buscar", UI_FONT_UI, fg, 255);
    bx = (float)g_bar_search.x + 13.0f;
    by = (float)MENUBAR_H * 0.5f;
    ui_ring(c, bx - 1.5f, by - 1.5f, 4.6f, 1.6f, fg, 255u);
    ui_capsule(c, bx + 2.0f, by + 2.0f, bx + 5.5f, by + 5.5f, 1.8f, fg, 255u);

    bx = (float)(g_bar_search.x + g_bar_search.w + 23);
    by = (float)MENUBAR_H * 0.5f + 5.0f;
    ui_arc(c, bx, by, 10.0f, 1.8f, 0.62f, fg, 255u);
    ui_arc(c, bx, by, 6.2f, 1.8f, 0.62f, fg, 255u);
    ui_circle(c, bx, by - 1.0f, 1.8f, fg, 255u);

    if (net_status() != NET_READY)
        ui_capsule(c, bx - 7, by - 12, bx + 7, by + 1, 1.8f, fg, 255u);

    clock_text(clock, sizeof(clock));
    ui_text(c, g_bar_clock.x + 8, ty, clock, UI_FONT_UI, fg, 255u);
}

/* ------------------------------------------------------------------- menús */

static void menu_close(void)
{
    if (!g_menu.open)
        return;
    ui_invalidate(g_menu.x - 30, g_menu.y - 30, g_menu.w + 60, g_menu.h + 60);
    g_menu.open = 0;
    g_menu.bar_item = -1;
    invalidate_bar();
}

void ui_menu_popup(int32_t x, int32_t y, const ui_menu_item_t *items, int n, ui_menu_cb cb, void *ctx)
{
    int i;
    int32_t maxw = 120, h = 10;

    menu_close();
    if (n > MENU_MAX)
        n = MENU_MAX;
    for (i = 0; i < n; i++) {
        g_menu.items[i] = items[i];
        if (items[i].label) {
            int32_t tw = ui_text_width(items[i].label, UI_FONT_UI) + 40;
            if (items[i].shortcut)
                tw += ui_text_width(items[i].shortcut, UI_FONT_UI) + 28;
            maxw = ui_max(maxw, tw);
            h += 24;
        } else {
            h += 11;
        }
    }
    g_menu.n = n;
    g_menu.w = maxw;
    g_menu.h = h;
    g_menu.x = ui_clampi(x, 4, W - maxw - 4);
    g_menu.y = ui_clampi(y, MENUBAR_H, H - h - 4);
    g_menu.hover = -1;
    g_menu.cb = cb;
    g_menu.ctx = ctx;
    g_menu.bar_item = -1;
    g_menu.open = 1;
    ui_invalidate(g_menu.x - 30, g_menu.y - 30, g_menu.w + 60, g_menu.h + 60);
}

static int menu_item_at(int32_t mx, int32_t my)
{
    int i;
    int32_t y = g_menu.y + 5;

    if (mx < g_menu.x || mx >= g_menu.x + g_menu.w)
        return -1;
    for (i = 0; i < g_menu.n; i++) {
        int32_t ih = g_menu.items[i].label ? 24 : 11;
        if (my >= y && my < y + ih)
            return (g_menu.items[i].label && !g_menu.items[i].disabled) ? i : -1;
        y += ih;
    }
    return -1;
}

static void draw_menu(ui_canvas_t *c)
{
    int i;
    int32_t y;

    if (!g_menu.open)
        return;
    ui_shadow(c, (float)g_menu.x, (float)g_menu.y, (float)g_menu.w, (float)g_menu.h, 12.0f,
              22.0f, 8.0f, 60u);
    panel_material(c, g_menu.x, g_menu.y, g_menu.w, g_menu.h, 12.0f, &UI_GLASS_MENU, 0xF5F8F6);
    ui_rrect_stroke(c, (float)g_menu.x, (float)g_menu.y, (float)g_menu.w, (float)g_menu.h,
                    12.0f, 1.0f, 0x000000u, 30u);

    y = g_menu.y + 5;
    for (i = 0; i < g_menu.n; i++) {
        const ui_menu_item_t *it = &g_menu.items[i];

        if (!it->label) {
            ui_fill(c, g_menu.x + 12, y + 5, g_menu.w - 24, 1, 0x000000u, 30u);
            y += 11;
            continue;
        }
        {
            uint32_t fg = it->disabled ? 0x9A9AA0u : UI_TEXT;
            if (i == g_menu.hover) {
                ui_rrect(c, (float)(g_menu.x + 5), (float)y, (float)(g_menu.w - 10), 24.0f, 7.0f,
                         UI_ACCENT, 255u);
                fg = 0xFFFFFFu;
            }
            ui_text(c, g_menu.x + 16, y + 3, it->label, UI_FONT_UI, fg, 255u);
            if (it->shortcut) {
                int32_t sw = ui_text_width(it->shortcut, UI_FONT_UI);
                ui_text(c, g_menu.x + g_menu.w - 16 - sw, y + 3, it->shortcut, UI_FONT_UI,
                        i == g_menu.hover ? 0xFFFFFFu : 0x8A8A90u, 255u);
            }
        }
        y += 24;
    }
}

/* acciones globales de menú */
enum {
    ACT_ABOUT = 1, ACT_SETTINGS, ACT_REBOOT, ACT_SHUTDOWN,
    ACT_APP_HIDE, ACT_APP_QUIT, ACT_NEW_WIN, ACT_CLOSE_WIN, ACT_OPEN_FILES, ACT_OPEN_TERMINAL,
    ACT_MINIMIZE, ACT_ZOOM, ACT_FRONT, ACT_HELP, ACT_SHORTCUTS,
    ACT_DESK_FOLDER, ACT_DESK_DOC, ACT_DESK_WALL, ACT_GLASS, ACT_WIN_BASE = 100
};

static void new_window_cmd(void)
{
    if (g_focus >= 0) {
        ui_window_t *w = &g_win[g_focus];
        const ui_app_t *a = ui_apps[w->app];
        if (a->new_cmd && a->new_cmd(w, g_focus))
            return;
        ui_open_app(w->app, NULL);
    } else {
        ui_open_app(APP_FILES, NULL);
    }
}

static void global_menu_cb(int id, void *ctx)
{
    char path[160];
    (void)ctx;

    switch (id) {
    case ACT_ABOUT:
        ui_open_app(APP_SETTINGS, "about");
        break;
    case ACT_SETTINGS:
        ui_open_app(APP_SETTINGS, NULL);
        break;
    case ACT_REBOOT:
        ui_reboot();
        break;
    case ACT_SHUTDOWN:
        ui_shutdown();
        break;
    case ACT_APP_HIDE:
        if (g_focus >= 0) {
            int app = g_win[g_focus].app, i;
            for (i = 0; i < UI_MAX_WIN; i++) {
                if (g_win[i].used && g_win[i].app == app && !g_win[i].minimized)
                    minimize_window(&g_win[i]);
            }
        }
        break;
    case ACT_APP_QUIT:
        if (g_focus >= 0)
            close_app(g_win[g_focus].app);
        break;
    case ACT_NEW_WIN:
        new_window_cmd();
        break;
    case ACT_CLOSE_WIN:
        if (g_focus >= 0)
            ui_close_window(&g_win[g_focus]);
        break;
    case ACT_OPEN_FILES:
        ui_open_app(APP_FILES, NULL);
        break;
    case ACT_OPEN_TERMINAL:
        ui_open_app(APP_TERMINAL, NULL);
        break;
    case ACT_MINIMIZE:
        if (g_focus >= 0)
            minimize_window(&g_win[g_focus]);
        break;
    case ACT_ZOOM:
        if (g_focus >= 0)
            toggle_zoom(&g_win[g_focus]);
        break;
    case ACT_FRONT: {
        int i;
        for (i = 0; i < UI_MAX_WIN; i++) {
            if (g_win[i].used && g_win[i].minimized) {
                g_win[i].minimized = 0;
                invalidate_win_full(&g_win[i]);
            }
        }
        break;
    }
    case ACT_HELP:
        fs_path(fs_find(fs_docs(), "Bienvenida.txt"), path, sizeof(path));
        ui_open_app(APP_NOTES, path);
        break;
    case ACT_SHORTCUTS:
        fs_path(fs_find(fs_docs(), "Atajos.txt"), path, sizeof(path));
        ui_open_app(APP_NOTES, path);
        break;
    case ACT_DESK_FOLDER: {
        char name[48];
        fs_unique_name(fs_desktop(), "Carpeta nueva", "", name, sizeof(name));
        fs_create(fs_desktop(), name, FS_DIR);
        ui_invalidate(0, 0, W, H);
        break;
    }
    case ACT_DESK_DOC: {
        char name[48];
        int f;
        fs_unique_name(fs_desktop(), "Documento", ".txt", name, sizeof(name));
        f = fs_create(fs_desktop(), name, FS_TEXT);
        if (f >= 0) {
            fs_path(f, path, sizeof(path));
            ui_open_app(APP_NOTES, path);
        }
        ui_invalidate(0, 0, W, H);
        break;
    }
    case ACT_DESK_WALL:
        ui_open_app(APP_SETTINGS, "wallpaper");
        break;
    case ACT_GLASS:
        (void)ui_set_liquid_glass(!ui_liquid_glass());
        ui_invalidate(0, 0, W, H);
        break;
    default:
        if (id >= ACT_WIN_BASE && id < ACT_WIN_BASE + UI_MAX_WIN) {
            ui_window_t *w = &g_win[id - ACT_WIN_BASE];
            if (w->used) {
                w->minimized = 0;
                focus_window(id - ACT_WIN_BASE);
                invalidate_win_full(w);
            }
        }
        break;
    }
}

static void open_bar_menu(int item)
{
    static char app_about[64], app_hide[64], app_quit[64];
    static char win_titles[UI_MAX_WIN][64];
    ui_menu_item_t items[MENU_MAX];
    int n = 0, i;
    const char *app = focused_app_name();

    switch (item) {
    case BAR_LOGO:
        items[n++] = (ui_menu_item_t){ "Acerca de MettaOS", ACT_ABOUT, 0, NULL };
        items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Ajustes del sistema…", ACT_SETTINGS, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Liquid Glass", ACT_GLASS, 0,
                                       ui_liquid_glass() ? "Activado" : "Desactivado" };
        items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Reiniciar", ACT_REBOOT, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Apagar", ACT_SHUTDOWN, 0, NULL };
        break;
    case BAR_APP:
        ui_strlcpy(app_about, "Acerca de ", sizeof(app_about));
        ui_strlcat(app_about, app, sizeof(app_about));
        ui_strlcpy(app_hide, "Ocultar ", sizeof(app_hide));
        ui_strlcat(app_hide, app, sizeof(app_hide));
        ui_strlcpy(app_quit, "Salir de ", sizeof(app_quit));
        ui_strlcat(app_quit, app, sizeof(app_quit));
        items[n++] = (ui_menu_item_t){ app_about, ACT_ABOUT, 0, NULL };
        items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Ajustes…", ACT_SETTINGS, 0, NULL };
        items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
        items[n++] = (ui_menu_item_t){ app_hide, ACT_APP_HIDE, g_focus < 0, "Ctrl H" };
        items[n++] = (ui_menu_item_t){ app_quit, ACT_APP_QUIT, g_focus < 0, "Ctrl Q" };
        break;
    case BAR_FILE:
        items[n++] = (ui_menu_item_t){ "Nueva ventana", ACT_NEW_WIN, 0, "Ctrl N" };
        items[n++] = (ui_menu_item_t){ "Abrir Archivos", ACT_OPEN_FILES, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Abrir Terminal", ACT_OPEN_TERMINAL, 0, NULL };
        items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Cerrar ventana", ACT_CLOSE_WIN, g_focus < 0, "Ctrl W" };
        break;
    case BAR_WINDOW:
        items[n++] = (ui_menu_item_t){ "Minimizar", ACT_MINIMIZE, g_focus < 0, "Ctrl M" };
        items[n++] = (ui_menu_item_t){ "Zoom", ACT_ZOOM, g_focus < 0, NULL };
        items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Traer todo al frente", ACT_FRONT, 0, NULL };
        for (i = 0; i < g_order_n && n < MENU_MAX - 1; i++) {
            ui_window_t *w = &g_win[g_order[i]];
            if (i == 0)
                items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
            ui_strlcpy(win_titles[g_order[i]], w->minimized ? "◦ " : "", 64);
            if (g_order[i] == g_focus)
                ui_strlcpy(win_titles[g_order[i]], "• ", 64);
            ui_strlcat(win_titles[g_order[i]], w->title, 64);
            items[n++] = (ui_menu_item_t){ win_titles[g_order[i]], ACT_WIN_BASE + g_order[i], 0, NULL };
        }
        break;
    case BAR_HELP:
        items[n++] = (ui_menu_item_t){ "Bienvenida a MettaOS", ACT_HELP, 0, NULL };
        items[n++] = (ui_menu_item_t){ "Atajos de teclado", ACT_SHORTCUTS, 0, NULL };
        break;
    default:
        return;
    }
    ui_menu_popup(g_bar_rects[item].x, MENUBAR_H + 2, items, n, global_menu_cb, NULL);
    g_menu.bar_item = item;
    invalidate_bar();
}

static void desktop_menu(int32_t x, int32_t y)
{
    ui_menu_item_t items[8];
    int n = 0;

    items[n++] = (ui_menu_item_t){ "Nueva carpeta", ACT_DESK_FOLDER, 0, NULL };
    items[n++] = (ui_menu_item_t){ "Nuevo documento", ACT_DESK_DOC, 0, NULL };
    items[n++] = (ui_menu_item_t){ NULL, 0, 0, NULL };
    items[n++] = (ui_menu_item_t){ "Cambiar fondo de pantalla…", ACT_DESK_WALL, 0, NULL };
    items[n++] = (ui_menu_item_t){ "Liquid Glass", ACT_GLASS, 0,
                                   ui_liquid_glass() ? "Activado" : "Desactivado" };
    items[n++] = (ui_menu_item_t){ "Acerca de MettaOS", ACT_ABOUT, 0, NULL };
    ui_menu_popup(x, y, items, n, global_menu_cb, NULL);
}

/* ---------------------------------------------------------------- búsqueda */

static ui_rect_t spot_bar(void)
{
    ui_rect_t r = { W / 2 - 300, 150, 600, 56 };
    return r;
}

static ui_rect_t spot_list(void)
{
    ui_rect_t b = spot_bar();
    ui_rect_t r = { b.x, b.y + b.h + 8, b.w, g_spot.n * 44 + 12 };
    return r;
}

static void spot_invalidate(void)
{
    ui_invalidate(W / 2 - 340, 110, 680, 480);
}

static void spot_update(void)
{
    int i, n = 0, f;

    if (g_spot.q[0]) {
        for (i = 0; i < APP_COUNT && n < SPOT_MAX; i++) {
            if (ui_str_icontains(ui_apps[i]->name, g_spot.q)) {
                g_spot.kind[n] = 0;
                g_spot.ref[n++] = i;
            }
        }
        for (f = 1; f < FS_NODE_CAP && n < SPOT_MAX; f++) {
            fs_node_t *nd = fs_node(f);
            if (nd && nd->type != FS_APP && ui_str_icontains(nd->name, g_spot.q)) {
                g_spot.kind[n] = 1;
                g_spot.ref[n++] = f;
            }
        }
    } else {
        for (i = 0; i < APP_COUNT && n < SPOT_MAX; i++) {
            if (i == APP_LINUX && !ui_linux_attached()) continue;
            g_spot.kind[n] = 0;
            g_spot.ref[n++] = i;
        }
    }
    g_spot.n = n;
    if (g_spot.sel >= n)
        g_spot.sel = n - 1;
    if (g_spot.sel < 0 && n > 0)
        g_spot.sel = 0;
    spot_invalidate();
}

static void spot_open(int on)
{
    g_spot.open = on;
    g_spot.q[0] = '\0';
    g_spot.sel = 0;
    g_spot.n = 0;
    if (on) spot_update();
    else spot_invalidate();
    invalidate_bar();
}

static void spot_activate(int i)
{
    if (i < 0 || i >= g_spot.n)
        return;
    spot_open(0);
    if (g_spot.kind[i] == 0)
        ui_open_app(g_spot.ref[i], NULL);
    else
        fs_open(g_spot.ref[i]);
}

static void draw_spotlight(ui_canvas_t *c)
{
    ui_rect_t b = spot_bar();
    int i;
    int32_t tx;

    ui_shadow(c, (float)b.x, (float)b.y, (float)b.w, (float)b.h, 28.0f, 34.0f, 14.0f, 75u);
    panel_material(c, b.x, b.y, b.w, b.h, 28.0f, &UI_GLASS_MENU, 0xF5F8F6);
    ui_rrect_stroke(c, (float)b.x, (float)b.y, (float)b.w, (float)b.h, 28.0f, 1.0f, 0x000000u, 30u);

    ui_ring(c, (float)b.x + 30.0f, (float)b.y + 26.0f, 8.0f, 2.2f, 0x6E6E73u, 255u);
    ui_capsule(c, (float)b.x + 36.0f, (float)b.y + 32.0f, (float)b.x + 42.0f, (float)b.y + 38.0f,
               2.6f, 0x6E6E73u, 255u);
    tx = b.x + 56;
    if (g_spot.q[0])
        tx = ui_text(c, tx, b.y + 14, g_spot.q, UI_FONT_TITLE, UI_TEXT, 255u);
    else
        ui_text(c, tx, b.y + 14, "Búsqueda", UI_FONT_TITLE, 0x8E8E93u, 255u);
    if ((g_now / 530u) % 2u == 0u)
        ui_fill(c, tx + 1, b.y + 15, 2, 26, UI_ACCENT, 255u);

    if (g_spot.n > 0) {
        ui_rect_t l = spot_list();

        ui_shadow(c, (float)l.x, (float)l.y, (float)l.w, (float)l.h, 18.0f, 30.0f, 12.0f, 60u);
        panel_material(c, l.x, l.y, l.w, l.h, 18.0f, &UI_GLASS_MENU, 0xF5F8F6);
        ui_rrect_stroke(c, (float)l.x, (float)l.y, (float)l.w, (float)l.h, 18.0f, 1.0f, 0x000000u, 30u);
        for (i = 0; i < g_spot.n; i++) {
            int32_t ry = l.y + 6 + i * 44;
            uint32_t fg = UI_TEXT, fg2 = UI_TEXT_2;
            const char *name, *kind;
            char path[160];
            int icon;

            if (i == g_spot.sel) {
                ui_rrect(c, (float)(l.x + 6), (float)ry, (float)(l.w - 12), 44.0f, 10.0f, UI_ACCENT, 255u);
                fg = fg2 = 0xFFFFFFu;
            }
            if (g_spot.kind[i] == 0) {
                name = ui_apps[g_spot.ref[i]]->name;
                icon = ui_apps[g_spot.ref[i]]->icon;
                kind = "Aplicación";
            } else {
                fs_node_t *nd = fs_node(g_spot.ref[i]);
                name = nd ? nd->name : "?";
                icon = fs_icon(g_spot.ref[i]);
                fs_path(nd ? nd->parent : 0, path, sizeof(path));
                kind = path;
            }
            ui_icon_draw(c, icon, (float)(l.x + 16), (float)(ry + 6), 32.0f);
            ui_text_fit(c, l.x + 60, ry + 12, name, UI_FONT_UI_BOLD, fg, 255u, l.w - 290);
            ui_text_fit(c, l.x + l.w - 220, ry + 12, kind, UI_FONT_UI, fg2, 255u, 200);
        }
    }
}

static ui_rect_t desk_icon_rect(int k)
{
    ui_rect_t r = { W - 108 - (k / 6) * 100, MENUBAR_H + 14 + (k % 6) * 96, 96, 90 };
    return r;
}

static int desk_icon_at(int32_t x, int32_t y)
{
    int k = 0, f = -1;

    while ((f = fs_child_next(fs_desktop(), f)) >= 0) {
        ui_rect_t r = desk_icon_rect(k);
        if (ui_rect_has(&r, x, y))
            return f;
        k++;
    }
    return -1;
}

static void draw_desktop_icons(ui_canvas_t *c)
{
    int k = 0, f = -1;

    while ((f = fs_child_next(fs_desktop(), f)) >= 0) {
        ui_rect_t r = desk_icon_rect(k);
        fs_node_t *nd = fs_node(f);
        int32_t tw;
        uint32_t fg = g_bar_light_text ? 0xFFFFFFu : UI_TEXT;

        if (f == g_desk_sel)
            ui_rrect(c, (float)(r.x + 16), (float)r.y, 64.0f, 62.0f, 8.0f, 0x000000u, 45u);
        ui_icon_draw(c, fs_icon(f), (float)(r.x + 20), (float)(r.y + 3), 56.0f);
        tw = ui_min(ui_text_width(nd->name, UI_FONT_SMALL), 92);
        if (f == g_desk_sel) {
            ui_rrect(c, (float)(r.x + 48 - tw / 2 - 5), (float)(r.y + 64), (float)(tw + 10), 18.0f,
                     5.0f, UI_ACCENT, 255u);
            fg = 0xFFFFFFu;
        }
        ui_text_fit(c, r.x + 48 - tw / 2, r.y + 65, nd->name, UI_FONT_SMALL, fg, 255u, 92);
        k++;
    }
}

/* ------------------------------------------------------------ dibujo ventana */

static void draw_window(ui_canvas_t *c, int wi)
{
    ui_window_t *w = &g_win[wi];
    const ui_app_t *a = ui_apps[w->app];
    int focused = (wi == g_focus);
    ui_rect_t vis = { w->x - SHADOW_PAD, w->y - SHADOW_PAD, w->w + SHADOW_PAD * 2, w->h + SHADOW_PAD * 2 };
    ui_rect_t clip = { c->cx0, c->cy0, c->cx1 - c->cx0, c->cy1 - c->cy0 };
    int32_t sx0 = c->cx0, sy0 = c->cy0, sx1 = c->cx1, sy1 = c->cy1;
    float fx = (float)w->x, fy = (float)w->y, fw = (float)w->w, fh = (float)w->h;
    int k, hover_lights;
    static const uint32_t k_light[3] = { 0xFF5F57u, 0xFEBC2Eu, 0x28C840u };

    if (!ui_rects_overlap(&vis, &clip))
        return;

    ui_shadow(c, fx, fy, fw, fh, WIN_RADIUS, focused ? 25.0f : 14.0f, focused ? 8.0f : 4.0f,
              focused ? 70u : 38u);
    ui_rect_t gr[4];
    int regions = a->glass ? a->glass(w, wi, gr, 4) : 0;
    if (g_glass_enabled && (a->dark || a->glass_body)) {
        ui_glass(c, w->x, w->y, w->w, w->h, WIN_RADIUS, &UI_GLASS_DARK);
    } else {
        if (g_glass_enabled && regions) {
            /* Leave the desktop behind the sidebar visible to its blur pass. */
            ui_rrect(c, fx, fy, fw, fh, WIN_RADIUS, a->bg, 78);
            c->cx0 = ui_max(sx0, w->x + gr[0].x + gr[0].w);
        }
        ui_rrect(c, fx, fy, fw, fh, WIN_RADIUS, a->bg, 255u);
        c->cx0 = sx0;
    }

    for (int i = 0; i < regions; i++)
        panel_material(c, w->x+gr[i].x, w->y+gr[i].y, gr[i].w, gr[i].h, 6,
                       &UI_GLASS_LIGHT, 0xEAF1ED);

    c->cx0 = ui_max(sx0, w->x);
    c->cy0 = ui_max(sy0, w->y);
    c->cx1 = ui_min(sx1, w->x + w->w);
    c->cy1 = ui_min(sy1, w->y + w->h);
    if (c->cx0 < c->cx1 && c->cy0 < c->cy1) {
        a->draw(w, wi, c, w->x, w->y, focused);
        if (a->show_title)
            ui_text_fit(c, w->x + w->w / 2 - ui_min(ui_text_width(w->title, UI_FONT_UI_BOLD), w->w - 200) / 2,
                        w->y + 14, w->title, UI_FONT_UI_BOLD,
                        a->dark ? (focused ? 0xF2F2F7u : 0x8E8E93u) : (focused ? 0x3A3A3Cu : 0xA1A1A6u),
                        255u, w->w - 200);
    }
    c->cx0 = sx0;
    c->cy0 = sy0;
    c->cx1 = sx1;
    c->cy1 = sy1;

    hover_lights = (g_lights_hover == wi);
    for (k = 0; k < 3; k++) {
        ui_rect_t r = light_rect(w, k);
        float cx = (float)r.x + 7.0f, cy = (float)r.y + 7.0f;
        uint32_t col = (focused || hover_lights) ? k_light[k] : (a->dark ? 0x55575Du : 0xD1D1D6u);

        ui_circle(c, cx, cy, 6.5f, col, 255u);
        ui_ring(c, cx, cy, 6.0f, 1.0f, 0x000000u, 22u);
        if (hover_lights) {
            uint32_t sym = 0x4A0F0Au;
            if (k == 0) {
                ui_capsule(c, cx - 2.6f, cy - 2.6f, cx + 2.6f, cy + 2.6f, 1.4f, sym, 200u);
                ui_capsule(c, cx - 2.6f, cy + 2.6f, cx + 2.6f, cy - 2.6f, 1.4f, sym, 200u);
            } else if (k == 1) {
                ui_capsule(c, cx - 3.2f, cy, cx + 3.2f, cy, 1.5f, 0x5A3D00u, 200u);
            } else {
                ui_capsule(c, cx - 3.2f, cy, cx + 3.2f, cy, 1.5f, 0x064A12u, 200u);
                ui_capsule(c, cx, cy - 3.2f, cx, cy + 3.2f, 1.5f, 0x064A12u, 200u);
            }
        }
    }

    ui_rrect_stroke(c, fx, fy, fw, fh, WIN_RADIUS, 1.0f, a->dark || a->glass_body ? 0xFFFFFFu : 0x000000u,
                    a->dark || a->glass_body ? 40u : (focused ? 50u : 35u));
}

/* ---------------------------------------------------------------- render */

static int collect_glass(ui_rect_t *out, int max)
{
    int n = 0;
    if (!g_glass_enabled) return 0;
    float ix[DOCK_N], iy[DOCK_N], sizes[DOCK_N];
    dock_geom(ix, iy, sizes, &out[n++]);
    out[n++] = (ui_rect_t){0, 0, W, MENUBAR_H};
    for (int j = 0; j < g_order_n && n < max - 3; j++) {
        int slot = g_order[j];
        ui_window_t *w = &g_win[slot];
        const ui_app_t *a = ui_apps[w->app];
        if (w->minimized) continue;
        if (a->dark || a->glass_body)
            out[n++] = (ui_rect_t){w->x, w->y, w->w, w->h};
        else if (a->glass) {
            int count = a->glass(w, slot, &out[n], max - 3 - n);
            for (int i = 0; i < count; i++, n++) {
                out[n].x += w->x;
                out[n].y += w->y;
            }
        }
    }
    if (g_menu.open && n < max)
        out[n++] = (ui_rect_t){ g_menu.x, g_menu.y, g_menu.w, g_menu.h };
    if (g_spot.open && n < max - 1) {
        out[n++] = spot_bar();
        if (g_spot.n > 0)
            out[n++] = spot_list();
    }
    return n;
}

int ui_render(int32_t *ox, int32_t *oy, int32_t *ow, int32_t *oh)
{
    ui_rect_t clip = g_dirty, glass[32];
    int ng, i, changed, iter;
    int32_t y;

    if (clip.w <= 0 || clip.h <= 0)
        return 0;

    ng = collect_glass(glass, 32);
    for (iter = 0; iter < ng; iter++) {
        changed = 0;
        for (i = 0; i < ng; i++) {
            ui_rect_t before = clip;
            if (!ui_rects_overlap(&clip, &glass[i]))
                continue;
            ui_rect_union(&clip, &glass[i]);
            if (clip.x != before.x || clip.y != before.y || clip.w != before.w || clip.h != before.h)
                changed = 1;
        }
        if (!changed)
            break;
    }
    clip.x = ui_max(clip.x, 0);
    clip.y = ui_max(clip.y, 0);
    clip.w = ui_min(clip.w, W - clip.x);
    clip.h = ui_min(clip.h, H - clip.y);
    g_dirty.w = 0;
    g_dirty.h = 0;
    if (clip.w <= 0 || clip.h <= 0)
        return 0;

    ui_canvas_clip(&g_cv, clip.x, clip.y, clip.w, clip.h);
    for (y = clip.y; y < clip.y + clip.h; y++)
        memcpy(g_cv.px + (int64_t)y * g_cv.stride + clip.x, g_wall + (int64_t)y * W + clip.x,
               (size_t)clip.w * 4u);

    draw_desktop_icons(&g_cv);
    for (i = 0; i < g_order_n; i++) {
        if (!g_win[g_order[i]].minimized)
            draw_window(&g_cv, g_order[i]);
    }
    draw_dock(&g_cv);
    draw_menubar(&g_cv);
    draw_menu(&g_cv);
    if (g_spot.open)
        draw_spotlight(&g_cv);

    *ox = clip.x;
    *oy = clip.y;
    *ow = clip.w;
    *oh = clip.h;
    return 1;
}

/* ---------------------------------------------------------------- cursor */

static const char *const k_cursor[19] = {
    "W           ", "WW          ", "WBW         ", "WBBW        ", "WBBBW       ",
    "WBBBBW      ", "WBBBBBW     ", "WBBBBBBW    ", "WBBBBBBBW   ", "WBBBBBBBBW  ",
    "WBBBBBBBBBW ", "WBBBBBBWWWWW", "WBBBWBBW    ", "WBBWWBBW    ", "WBW  WBBW   ",
    "WW   WBBW   ", "W     WBBW  ", "      WBBW  ", "       WW   ",
};

void ui_cursor_draw(uint32_t *back, int32_t stride, int32_t width, int32_t height,
                    int32_t x, int32_t y)
{
    int32_t r, k, pass;

    for (pass = 0; pass < 2; pass++) {
        for (r = 0; r < 19; r++) {
            for (k = 0; k < 12; k++) {
                char ch = k_cursor[r][k];
                int32_t px = x + k + (pass == 0 ? 1 : 0);
                int32_t py = y + r + (pass == 0 ? 2 : 0);
                uint32_t *d;

                if (ch == ' ' || px < 0 || py < 0 || px >= width || py >= height)
                    continue;
                d = back + (int64_t)py * stride + px;
                if (pass == 0)
                    *d = ui_mix(*d, 0x000000u, 70u);
                else
                    *d = (ch == 'W') ? 0xFFFFFFu : 0x000000u;
            }
        }
    }
}

/* --------------------------------------------------------------- teclado */

static uint32_t combine_dead(uint32_t dead, char ch)
{
    static const char vowels[] = "aeiouAEIOU";
    static const uint16_t acute[] = { 0xE1, 0xE9, 0xED, 0xF3, 0xFA, 0xC1, 0xC9, 0xCD, 0xD3, 0xDA };
    static const uint16_t diaer[] = { 0xE4, 0xEB, 0xEF, 0xF6, 0xFC, 0xC4, 0xCB, 0xCF, 0xD6, 0xDC };
    int i;

    for (i = 0; vowels[i]; i++) {
        if (vowels[i] == ch)
            return dead == 0xB4u ? acute[i] : diaer[i];
    }
    return 0;
}

static int key_text(const input_event_t *ev, char *out)
{
    static const char k_letters[] = "qwertyuiopasdfghjklzxcvbnm";
    static const input_keycode_t k_letter_keys[26] = {
        INPUT_KEY_Q, INPUT_KEY_W, INPUT_KEY_E, INPUT_KEY_R, INPUT_KEY_T, INPUT_KEY_Y, INPUT_KEY_U,
        INPUT_KEY_I, INPUT_KEY_O, INPUT_KEY_P, INPUT_KEY_A, INPUT_KEY_S, INPUT_KEY_D, INPUT_KEY_F,
        INPUT_KEY_G, INPUT_KEY_H, INPUT_KEY_J, INPUT_KEY_K, INPUT_KEY_L, INPUT_KEY_Z, INPUT_KEY_X,
        INPUT_KEY_C, INPUT_KEY_V, INPUT_KEY_B, INPUT_KEY_N, INPUT_KEY_M,
    };
    static const char k_num_us[] = "!@#$%^&*()";
    static const char k_num_la[] = "!\"#$%&/()=";
    input_keycode_t key = ev->u.key.keycode;
    int shift = (ev->u.key.modifiers & INPUT_MOD_SHIFT) != 0;
    int la = (g_kbd_layout == 0);
    uint32_t cp = 0;
    int i;

    out[0] = '\0';
    if (ev->u.key.modifiers & INPUT_MOD_CTRL)
        return 0;
    if ((ev->u.key.modifiers & INPUT_MOD_ALT) && !g_altgr)
        return 0;

    for (i = 0; i < 26; i++) {
        if (k_letter_keys[i] == key) {
            char ch = k_letters[i];
            int upper = shift ^ g_caps;
            if (g_altgr && la && key == INPUT_KEY_Q) {
                cp = '@';
                break;
            }
            if (upper)
                ch = (char)(ch - 32);
            cp = (uint32_t)(unsigned char)ch;
            if (g_dead) {
                uint32_t comb = combine_dead(g_dead, ch);
                g_dead = 0;
                if (comb)
                    cp = comb;
            }
            break;
        }
    }

    if (!cp && key >= INPUT_KEY_1 && key <= INPUT_KEY_0) {
        int d = (int)(key - INPUT_KEY_1);
        if (shift)
            cp = (uint32_t)(unsigned char)(la ? k_num_la[d] : k_num_us[d]);
        else
            cp = (d == 9) ? '0' : (uint32_t)('1' + d);
    }

    if (!cp) {
        switch (key) {
        case INPUT_KEY_SPACE:
            cp = ' ';
            if (g_dead) {
                cp = g_dead == 0xB4u ? '\'' : '"';
                g_dead = 0;
            }
            break;
        case INPUT_KEY_MINUS:
            cp = la ? (g_altgr ? '\\' : (shift ? '?' : '\'')) : (shift ? '_' : '-');
            break;
        case INPUT_KEY_EQUAL:
            cp = la ? (shift ? 0xA1u : 0xBFu) : (shift ? '+' : '=');
            break;
        case INPUT_KEY_LEFTBRACE:
            if (la) {
                g_dead = shift ? 0xA8u : 0xB4u;
                return 0;
            }
            cp = shift ? '{' : '[';
            break;
        case INPUT_KEY_RIGHTBRACE:
            cp = la ? (g_altgr ? '~' : (shift ? '*' : '+')) : (shift ? '}' : ']');
            break;
        case INPUT_KEY_SEMICOLON:
            cp = la ? (shift ? 0xD1u : 0xF1u) : (shift ? ':' : ';');
            break;
        case INPUT_KEY_APOSTROPHE:
            cp = la ? (g_altgr ? '^' : (shift ? '[' : '{')) : (shift ? '"' : '\'');
            break;
        case INPUT_KEY_BACKSLASH:
            cp = la ? (g_altgr ? '`' : (shift ? ']' : '}')) : (shift ? '|' : '\\');
            break;
        case INPUT_KEY_GRAVE:
            cp = la ? (shift ? 0xB0u : '|') : (shift ? '~' : '`');
            break;
        case INPUT_KEY_COMMA:
            cp = shift ? (la ? ';' : '<') : ',';
            break;
        case INPUT_KEY_DOT:
            cp = shift ? (la ? ':' : '>') : '.';
            break;
        case INPUT_KEY_SLASH:
            cp = la ? (shift ? '_' : '-') : (shift ? '?' : '/');
            break;
        case INPUT_KEY_UNKNOWN:
            if (ev->u.key.scancode == 0x56u)
                cp = shift ? '>' : '<';
            break;
        default:
            break;
        }
    }

    if (!cp)
        return 0;
    if (g_dead) {
        g_dead = 0;
    }
    out[ui_utf8_encode(cp, out)] = '\0';
    return 1;
}

static void spot_key(const input_event_t *ev, const char *text)
{
    input_keycode_t key = ev->u.key.keycode;
    int32_t n = (int32_t)strlen(g_spot.q);

    switch (key) {
    case INPUT_KEY_ESC:
        spot_open(0);
        return;
    case INPUT_KEY_ENTER:
        spot_activate(g_spot.sel);
        return;
    case INPUT_KEY_UP:
        if (g_spot.sel > 0)
            g_spot.sel--;
        spot_invalidate();
        return;
    case INPUT_KEY_DOWN:
        if (g_spot.sel < g_spot.n - 1)
            g_spot.sel++;
        spot_invalidate();
        return;
    case INPUT_KEY_BACKSPACE:
        if (n > 0)
            g_spot.q[ui_utf8_prev(g_spot.q, n)] = '\0';
        spot_update();
        return;
    default:
        break;
    }
    if (text[0] && n + (int32_t)strlen(text) < (int32_t)sizeof(g_spot.q) - 1) {
        ui_strlcat(g_spot.q, text, sizeof(g_spot.q));
        g_spot.sel = 0;
        spot_update();
    }
}

static void handle_key(const input_event_t *ev)
{
    input_keycode_t key = ev->u.key.keycode;
    uint8_t mods = ev->u.key.modifiers;
    int ctrl = (mods & INPUT_MOD_CTRL) != 0;
    char text[8];

    if (!g_spot.open && !g_menu.open && g_focus >= 0 && g_win[g_focus].app == APP_LINUX &&
        !(ctrl && key == INPUT_KEY_SPACE) && !((mods & INPUT_MOD_ALT) && key == INPUT_KEY_TAB)) {
        ui_linux_key(g_focus, ev);
        return;
    }
    if (key == INPUT_KEY_RIGHTALT) {
        g_altgr = 1;
        return;
    }
    if (key == INPUT_KEY_CAPSLOCK) {
        g_caps ^= 1;
        return;
    }
    if (key == INPUT_KEY_LEFTSHIFT || key == INPUT_KEY_RIGHTSHIFT || key == INPUT_KEY_LEFTCTRL ||
        key == INPUT_KEY_RIGHTCTRL || key == INPUT_KEY_LEFTALT)
        return;

    if (ctrl && key == INPUT_KEY_SPACE) {
        if (g_focus >= 0 && g_win[g_focus].app == APP_LINUX) ui_linux_release(g_focus);
        spot_open(!g_spot.open);
        return;
    }
    if ((mods & INPUT_MOD_ALT) && !g_altgr && key == INPUT_KEY_TAB) {
        cycle_focus();
        return;
    }

    key_text(ev, text);

    if (g_spot.open) {
        spot_key(ev, text);
        return;
    }
    if (g_menu.open) {
        if (key == INPUT_KEY_ESC)
            menu_close();
        return;
    }
    if (ctrl) {
        switch (key) {
        case INPUT_KEY_W:
            if (g_focus >= 0)
                ui_close_window(&g_win[g_focus]);
            return;
        case INPUT_KEY_Q:
            if (g_focus >= 0)
                close_app(g_win[g_focus].app);
            return;
        case INPUT_KEY_M:
            if (g_focus >= 0)
                minimize_window(&g_win[g_focus]);
            return;
        case INPUT_KEY_H:
            global_menu_cb(ACT_APP_HIDE, NULL);
            return;
        case INPUT_KEY_N:
            new_window_cmd();
            return;
        default:
            break;
        }
    }

    if (g_focus >= 0) {
        ui_window_t *w = &g_win[g_focus];
        const ui_app_t *a = ui_apps[w->app];
        if (a->key)
            a->key(w, g_focus, ev, text);
    } else if (g_desk_sel >= 0 && key == INPUT_KEY_ENTER) {
        fs_open(g_desk_sel);
    }
}

/* ----------------------------------------------------------------- ratón */

static void set_lights_hover(int wi)
{
    if (wi == g_lights_hover)
        return;
    if (g_lights_hover >= 0 && g_win[g_lights_hover].used)
        ui_invalidate(g_win[g_lights_hover].x, g_win[g_lights_hover].y, 90, 46);
    g_lights_hover = wi;
    if (wi >= 0)
        ui_invalidate(g_win[wi].x, g_win[wi].y, 90, 46);
}

static void pointer_hover(void)
{
    int wi, dh, bh = -1, i;

    if (g_menu.open) {
        int h = menu_item_at(g_mx, g_my);
        if (h != g_menu.hover) {
            g_menu.hover = h;
            ui_invalidate(g_menu.x, g_menu.y, g_menu.w, g_menu.h);
        }
        if (g_menu.bar_item >= 0 && g_my < MENUBAR_H) {
            for (i = 0; i < BAR_ITEMS; i++) {
                if (ui_rect_has(&g_bar_rects[i], g_mx, g_my) && i != g_menu.bar_item) {
                    open_bar_menu(i);
                    break;
                }
            }
        }
        return;
    }

    if (g_my < MENUBAR_H) {
        for (i = 0; i < BAR_ITEMS; i++) {
            if (ui_rect_has(&g_bar_rects[i], g_mx, g_my))
                bh = i;
        }
    }
    if (bh != g_bar_hover) {
        g_bar_hover = bh;
        invalidate_bar();
    }

    dh = dock_hit(g_mx, g_my);
    if (dh < 0)
        dh = -1;
    if (dh != g_dock_hover) {
        g_dock_hover = dh;
        invalidate_dock();
    }

    wi = (dock_hit(g_mx, g_my) != -2 || g_my < MENUBAR_H) ? -1 : window_at(g_mx, g_my, 0);
    set_lights_hover((wi >= 0 && lights_group_hit(&g_win[wi], g_mx, g_my)) ? wi : -1);

    if (wi != g_hover_win) {
        if (g_hover_win >= 0 && g_win[g_hover_win].used && ui_apps[g_win[g_hover_win].app]->mouse)
            ui_apps[g_win[g_hover_win].app]->mouse(&g_win[g_hover_win], g_hover_win, UI_M_LEAVE, 0, 0, 0, 0);
        g_hover_win = wi;
    }
    if (wi >= 0 && ui_apps[g_win[wi].app]->mouse)
        ui_apps[g_win[wi].app]->mouse(&g_win[wi], wi, UI_M_HOVER, g_mx - g_win[wi].x,
                                      g_my - g_win[wi].y, 0, 0);
}

static void pointer_move(void)
{
    ui_window_t *w;

    if (g_drag == DRAG_NONE || g_drag_win < 0 || !g_win[g_drag_win].used) {
        pointer_hover();
        return;
    }
    w = &g_win[g_drag_win];

    switch (g_drag) {
    case DRAG_MOVE: {
        int32_t nx = g_drag_start.x + (g_mx - g_drag_mx);
        int32_t ny = g_drag_start.y + (g_my - g_drag_my);

        ny = ui_clampi(ny, MENUBAR_H, H - 60);
        nx = ui_clampi(nx, -w->w + 100, W - 100);
        if (nx != w->x || ny != w->y) {
            invalidate_win_full(w);
            if (w->maximized) {
                w->maximized = 0;
            }
            w->x = nx;
            w->y = ny;
            invalidate_win_full(w);
        }
        break;
    }
    case DRAG_RESIZE: {
        const ui_app_t *a = ui_apps[w->app];
        int32_t dx = g_mx - g_drag_mx, dy = g_my - g_drag_my;
        ui_rect_t r = g_drag_start;

        if (g_resize_edges & 2)
            r.w = ui_max(a->min_w, g_drag_start.w + dx);
        if (g_resize_edges & 1) {
            r.w = ui_max(a->min_w, g_drag_start.w - dx);
            r.x = g_drag_start.x + g_drag_start.w - r.w;
        }
        if (g_resize_edges & 4)
            r.h = ui_max(a->min_h, g_drag_start.h + dy);
        r.h = ui_min(r.h, H - r.y);
        if (r.x != w->x || r.w != w->w || r.h != w->h) {
            invalidate_win_full(w);
            w->x = r.x;
            w->w = r.w;
            w->h = r.h;
            invalidate_win_full(w);
        }
        break;
    }
    case DRAG_APP:
        if (ui_apps[w->app]->mouse)
            ui_apps[w->app]->mouse(w, g_drag_win, UI_M_DRAG, g_mx - w->x, g_my - w->y, 0, 0);
        break;
    case DRAG_LIGHT:
    default:
        break;
    }
}

static void pointer_down(int btn)
{
    int wi, di, i;
    ui_window_t *w;

    if (btn == INPUT_BTN_LEFT) {
        if (g_now - g_last_click_ms < 420u && ui_absf((float)(g_mx - g_last_click_x)) < 5.0f &&
            ui_absf((float)(g_my - g_last_click_y)) < 5.0f)
            g_click_count++;
        else
            g_click_count = 1;
        g_last_click_ms = g_now;
        g_last_click_x = g_mx;
        g_last_click_y = g_my;
    }

    if (g_spot.open) {
        ui_rect_t b = spot_bar(), l = spot_list();
        if (g_spot.n > 0 && ui_rect_has(&l, g_mx, g_my)) {
            spot_activate((g_my - l.y - 6) / 44);
            return;
        }
        if (!ui_rect_has(&b, g_mx, g_my))
            spot_open(0);
        return;
    }

    if (g_menu.open) {
        int bar_click = -1;
        ui_menu_cb cb;
        void *ctx;
        int id;

        if (btn == INPUT_BTN_LEFT || btn == INPUT_BTN_RIGHT) {
            int it = menu_item_at(g_mx, g_my);
            if (it >= 0) {
                cb = g_menu.cb;
                ctx = g_menu.ctx;
                id = g_menu.items[it].id;
                menu_close();
                if (cb)
                    cb(id, ctx);
                return;
            }
            if (g_mx >= g_menu.x && g_mx < g_menu.x + g_menu.w && g_my >= g_menu.y &&
                g_my < g_menu.y + g_menu.h)
                return;
        }
        if (g_my < MENUBAR_H) {
            for (i = 0; i < BAR_ITEMS; i++) {
                if (ui_rect_has(&g_bar_rects[i], g_mx, g_my))
                    bar_click = i;
            }
        }
        i = g_menu.bar_item;
        menu_close();
        if (bar_click >= 0 && bar_click != i)
            open_bar_menu(bar_click);
        return;
    }

    if (g_my < MENUBAR_H) {
        bar_layout();
        for (i = 0; i < BAR_ITEMS; i++) {
            if (ui_rect_has(&g_bar_rects[i], g_mx, g_my)) {
                open_bar_menu(i);
                return;
            }
        }
        if (g_mx >= g_bar_search.x + g_bar_search.w && g_mx < g_bar_clock.x) {
            ui_open_app(APP_SETTINGS, "network");
        } else if (ui_rect_has(&g_bar_search, g_mx, g_my)) {
            spot_open(1);
        }
        return;
    }

    di = dock_hit(g_mx, g_my);
    if (di >= 0) {
        if (btn == INPUT_BTN_LEFT)
            dock_click(di);
        return;
    }
    if (di == -1)
        return;

    wi = window_at(g_mx, g_my, 4);
    if (wi >= 0) {
        int32_t lx, ly, light, edges;

        w = &g_win[wi];
        focus_window(wi);
        lx = g_mx - w->x;
        ly = g_my - w->y;

        if (btn == INPUT_BTN_LEFT) {
            light = lights_hit(w, g_mx, g_my);
            if (light >= 0) {
                g_drag = DRAG_LIGHT;
                g_drag_win = wi;
                g_light_pressed = light;
                return;
            }
            edges = resize_edges_at(w, g_mx, g_my);
            if (edges) {
                g_drag = DRAG_RESIZE;
                g_drag_win = wi;
                g_resize_edges = edges;
                g_drag_mx = g_mx;
                g_drag_my = g_my;
                g_drag_start = (ui_rect_t){ w->x, w->y, w->w, w->h };
                return;
            }
        }

        g_consumed = 0;
        if (ui_apps[w->app]->mouse && lx >= 0 && ly >= 0 && lx < w->w && ly < w->h)
            ui_apps[w->app]->mouse(w, wi, UI_M_DOWN, lx, ly, btn, g_click_count);
        if (!w->used)
            return;

        if (btn == INPUT_BTN_LEFT) {
            if (!g_consumed && ly < UI_TITLEBAR_H) {
                if (g_click_count == 2) {
                    toggle_zoom(w);
                    return;
                }
                g_drag = DRAG_MOVE;
                g_drag_win = wi;
                g_drag_mx = g_mx;
                g_drag_my = g_my;
                g_drag_start = (ui_rect_t){ w->x, w->y, w->w, w->h };
            } else {
                g_drag = DRAG_APP;
                g_drag_win = wi;
            }
        }
        return;
    }

    /* escritorio */
    {
        int f = desk_icon_at(g_mx, g_my);

        if (g_desk_sel != f) {
            g_desk_sel = f;
            ui_invalidate(W - 320, MENUBAR_H, 320, H - MENUBAR_H);
        }
        if (g_focus >= 0) {
            int old = g_focus;
            g_focus = -1;
            invalidate_win_full(&g_win[old]);
            invalidate_bar();
        }
        if (btn == INPUT_BTN_LEFT && f >= 0 && g_click_count == 2)
            fs_open(f);
        if (btn == INPUT_BTN_RIGHT && f < 0)
            desktop_menu(g_mx, g_my);
        if (btn == INPUT_BTN_RIGHT && f >= 0) {
            static const ui_menu_item_t k_items[] = {
                { "Abrir", 1, 0, NULL },
                { NULL, 0, 0, NULL },
                { "Mover a la Papelera", 2, 0, NULL },
            };
            ui_menu_popup(g_mx, g_my, k_items, 3, desk_item_menu_cb, (void *)(intptr_t)f);
        }
    }
}

static void desk_item_menu_cb(int id, void *ctx)
{
    int f = (int)(intptr_t)ctx;

    if (id == 1)
        fs_open(f);
    else if (id == 2) {
        fs_move(f, fs_trash());
        g_desk_sel = -1;
        ui_invalidate(0, 0, W, H);
    }
}

static void pointer_up(int btn)
{
    ui_window_t *w;

    if (btn != INPUT_BTN_LEFT && g_focus >= 0 && g_win[g_focus].app == APP_LINUX) {
        w = &g_win[g_focus];
        ui_apps[APP_LINUX]->mouse(w, g_focus, UI_M_UP, g_mx-w->x, g_my-w->y, btn, 0);
    }
    if (g_drag_win >= 0 && g_win[g_drag_win].used) {
        w = &g_win[g_drag_win];
        if (g_drag == DRAG_LIGHT && btn == INPUT_BTN_LEFT) {
            int k = lights_hit(w, g_mx, g_my);
            if (k == g_light_pressed) {
                if (k == 0)
                    ui_close_window(w);
                else if (k == 1)
                    minimize_window(w);
                else
                    toggle_zoom(w);
            }
        } else if (g_drag == DRAG_APP && ui_apps[w->app]->mouse) {
            ui_apps[w->app]->mouse(w, g_drag_win, UI_M_UP, g_mx - w->x, g_my - w->y, btn, 0);
        }
    }
    g_drag = DRAG_NONE;
    g_drag_win = -1;
    pointer_hover();
}

/* ------------------------------------------------------------------- API */

void ui_event(const input_event_t *ev, uint64_t now_ms)
{
    g_now = now_ms;
    switch (ev->type) {
    case INPUT_EV_MOUSE_MOVE:
        g_mx = ev->u.mouse.x;
        g_my = ev->u.mouse.y;
        pointer_move();
        break;
    case INPUT_EV_MOUSE_BUTTON_DOWN:
        if (ev->u.btn.button == INPUT_BTN_LEFT)
            g_btn_left = 1;
        pointer_down((int)ev->u.btn.button);
        break;
    case INPUT_EV_MOUSE_BUTTON_UP:
        if (ev->u.btn.button == INPUT_BTN_LEFT)
            g_btn_left = 0;
        pointer_up((int)ev->u.btn.button);
        break;
    case INPUT_EV_MOUSE_WHEEL: {
        int wi = window_at(g_mx, g_my, 0);
        if (wi >= 0 && ui_apps[g_win[wi].app]->wheel)
            ui_apps[g_win[wi].app]->wheel(&g_win[wi], wi, ev->u.btn.wheel_delta);
        break;
    }
    case INPUT_EV_KEY_DOWN:
        handle_key(ev);
        break;
    case INPUT_EV_KEY_UP:
        if (g_focus >= 0 && g_win[g_focus].app == APP_LINUX) ui_linux_key(g_focus, ev);
        if (ev->u.key.keycode == INPUT_KEY_RIGHTALT)
            g_altgr = 0;
        break;
    default:
        break;
    }
}

void ui_tick(uint64_t now_ms)
{
    int i;

    static uint64_t last_sync;
    if (now_ms - last_sync >= 250) {
        fs_sync();
        last_sync = now_ms;
    }
    ui_linux_poll(now_ms);
    if (net_poll(now_ms)) invalidate_bar();
    g_now = now_ms;
    if (g_now - g_last_rtc >= 1000u || g_last_rtc == 0u) {
        ui_time_t t;
        ui_rtc_read(&t);
        if (t.minute != g_time.minute || t.hour != g_time.hour || t.day != g_time.day) {
            g_time = t;
            invalidate_bar();
        }
        g_time = t;
        g_last_rtc = g_now ? g_now : 1u;
    }

    dock_update_targets(0);

    if (g_spot.open && (g_now / 530u) != ((g_now - 10u) / 530u))
        spot_invalidate();

    for (i = 0; i < g_order_n; i++) {
        ui_window_t *w = &g_win[g_order[i]];
        const ui_app_t *a = ui_apps[w->app];
        if (!w->minimized && a->tick)
            a->tick(w, g_order[i], g_now);
    }
}

int ui_init(uint32_t *scene, int32_t width, int32_t height)
{
    int i;

    W = width;
    H = height;
    g_cv.px = scene;
    g_cv.w = width;
    g_cv.h = height;
    g_cv.stride = width;
    ui_canvas_clip(&g_cv, 0, 0, width, height);

    for (i = 0; i < DOCK_N; i++)
        g_dock_scale[i] = 1.0f;
    g_menu.bar_item = -1;
    g_kbd_layout = 0;
    net_init();
    ui_linux_init();
    fs_init();
    g_glass_enabled = fs_read_glass();
    ui_rtc_read(&g_time);
    ui_wallpaper_set(0);

    ui_invalidate(0, 0, W, H);
    return 0;
}

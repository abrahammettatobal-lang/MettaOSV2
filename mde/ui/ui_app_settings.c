/*
 * MettaOS — Ajustes del sistema.
 */
#include "ui_apps_common.h"

#include <interrupt/pit.h>
#include <network.h>
#include <wifi.h>

#define SIDE_X  8
#define SIDE_W  200
#define PANE_X  (SIDE_X + SIDE_W + 24)
#define TH_W    200
#define TH_H    125

enum { P_WALL = 0, P_KBD, P_NET, P_ABOUT, P_COUNT };

typedef struct {
    int pane;
    int hover_side;
    int hover_item;
    int temporary_glass;
} settings_t;

static settings_t g_s;
static uint32_t   g_thumbs[UI_WALL_COUNT][TH_W * TH_H];
static int        g_thumbs_ready;

static const char *const PANE_NAMES[P_COUNT] = { "Apariencia", "Teclado", "Red", "Acerca de" };
static const int PANE_ICONS[P_COUNT] = { UI_ICON_FILES, UI_ICON_NOTES, UI_ICON_NETWORK, UI_ICON_ABOUT };

static void settings_open(ui_window_t *w, int slot, const char *arg)
{
    (void)slot;
    g_s.pane = P_WALL;
    if (arg && ui_streq(arg, "about"))
        g_s.pane = P_ABOUT;
    else if (arg && ui_streq(arg, "keyboard"))
        g_s.pane = P_KBD;
    else if (arg && ui_streq(arg, "network"))
        g_s.pane = P_NET;
    g_s.hover_side = -1;
    g_s.hover_item = -1;
    ui_strlcpy(w->title, "Ajustes", sizeof(w->title));
}

static void ensure_thumbs(void)
{
    int i;

    if (g_thumbs_ready)
        return;
    for (i = 0; i < UI_WALL_COUNT; i++)
        ui_wall_render(i, g_thumbs[i], TH_W, TH_H, TH_W, 1024, 768);
    g_thumbs_ready = 1;
}

static void blit_thumb(ui_canvas_t *c, int idx, int32_t x, int32_t y)
{
    int32_t yy, xx;
    const float r = 10.0f;

    for (yy = 0; yy < TH_H; yy++) {
        int32_t py = y + yy;
        if (py < c->cy0 || py >= c->cy1)
            continue;
        for (xx = 0; xx < TH_W; xx++) {
            int32_t px = x + xx;
            float dx = 0.0f, dy = 0.0f;
            if (px < c->cx0 || px >= c->cx1)
                continue;
            if (xx < r)
                dx = r - xx - 0.5f;
            else if (xx > TH_W - r)
                dx = xx + 0.5f - (TH_W - r);
            if (yy < r)
                dy = r - yy - 0.5f;
            else if (yy > TH_H - r)
                dy = yy + 0.5f - (TH_H - r);
            if (dx * dx + dy * dy > r * r)
                continue;
            c->px[(int64_t)py * c->stride + px] = g_thumbs[idx][yy * TH_W + xx];
        }
    }
}

static void row_label(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, const char *k, const char *v)
{
    ui_text(c, x, y, k, UI_FONT_UI, UI_TEXT_2, 255u);
    ui_text(c, x + w - ui_text_width(v, UI_FONT_UI), y, v, UI_FONT_UI, UI_TEXT, 255u);
}

static void settings_draw(ui_window_t *w, int slot, ui_canvas_t *c, int32_t ox, int32_t oy, int focused)
{
    int k;
    int32_t px = ox + PANE_X, pw = w->w - PANE_X - 28;
    (void)slot;
    (void)focused;

    for (k = 0; k < P_COUNT; k++) {
        int32_t iy = oy + 52 + k * 34;
        if (k == g_s.pane)
            ui_rrect(c, (float)(ox + SIDE_X + 8), (float)iy, (float)(SIDE_W - 16), 30.0f, 8.0f,
                     UI_ACCENT, 255u);
        else if (k == g_s.hover_side)
            ui_rrect(c, (float)(ox + SIDE_X + 8), (float)iy, (float)(SIDE_W - 16), 30.0f, 8.0f,
                     0x000000u, 14u);
        ui_icon_draw(c, PANE_ICONS[k], (float)(ox + SIDE_X + 16), (float)(iy + 4), 22.0f);
        ui_text(c, ox + SIDE_X + 46, iy + 6, PANE_NAMES[k], UI_FONT_UI,
                k == g_s.pane ? 0xFFFFFFu : UI_TEXT, 255u);
    }

    ui_text(c, px, oy + 14, PANE_NAMES[g_s.pane], UI_FONT_UI_BOLD, UI_TEXT, 255u);

    if (g_s.pane == P_WALL) {
        int cur = ui_wallpaper_get();
        int glass = ui_liquid_glass();
        int sy = oy + 46;
        ui_rrect(c, px, sy, pw, 47, 7, g_s.hover_item == UI_WALL_COUNT ? 0xDDEAE5 : 0xEDF3EF, 255);
        ui_text(c, px + 14, sy + 5, "Liquid Glass", UI_FONT_UI_BOLD, UI_TEXT, 255);
        ui_text(c, px + 14, sy + 25, g_s.temporary_glass ? "Cambio temporal: no se pudo guardar en MFS" :
                "Transparencia y desenfoque del escritorio", UI_FONT_SMALL, UI_TEXT_2, 255);
        ui_text(c, px + pw - 142, sy + 14, glass ? "Activado" : "Desactivado", UI_FONT_SMALL, UI_TEXT_2, 255);
        ui_rrect(c, px + pw - 58, sy + 11, 44, 24, 12, glass ? UI_ACCENT : 0x728580, 255);
        ui_circle(c, px + pw - (glass ? 26 : 46), sy + 23, 9, 0xFFFFFF, 255);
        ui_text(c, px, oy + 106, "Fondo de pantalla", UI_FONT_SMALL, UI_TEXT_2, 255);
        ensure_thumbs();
        for (k = 0; k < UI_WALL_COUNT; k++) {
            int32_t tx = px + (k % 2) * (TH_W + 24);
            int32_t ty = oy + 135 + (k / 2) * (TH_H + 40);
            ui_shadow(c, (float)tx, (float)ty, (float)TH_W, (float)TH_H, 10.0f, 8.0f, 3.0f, 60u);
            blit_thumb(c, k, tx, ty);
            if (k == cur)
                ui_rrect_stroke(c, (float)(tx - 4), (float)(ty - 4), (float)(TH_W + 8), (float)(TH_H + 8),
                                13.0f, 3.0f, UI_ACCENT, 255u);
            else if (k == g_s.hover_item)
                ui_rrect_stroke(c, (float)(tx - 4), (float)(ty - 4), (float)(TH_W + 8), (float)(TH_H + 8),
                                13.0f, 2.0f, 0x000000u, 40u);
            ui_text_center(c, tx + TH_W / 2, ty + TH_H + 10, ui_wall_name(k), UI_FONT_SMALL,
                           k == cur ? UI_TEXT : UI_TEXT_2, 255u);
        }
    } else if (g_s.pane == P_KBD) {
        static const char *names[2] = { "Español (Latinoamérica)", "Inglés (EE. UU.)" };
        int cur = ui_kbd_layout_get();
        ui_rrect(c, (float)px, (float)(oy + 56), (float)pw, 100.0f, 12.0f, 0xF5F5F7u, 255u);
        for (k = 0; k < 2; k++) {
            int32_t ry = oy + 56 + k * 50;
            if (k == 1)
                ui_fill(c, px + 16, ry, pw - 32, 1, UI_SEPARATOR, 255u);
            if (k == g_s.hover_item)
                ui_rrect(c, (float)(px + 4), (float)(ry + 4), (float)(pw - 8), 42.0f, 9.0f, 0x000000u, 10u);
            ui_text(c, px + 18, ry + 15, names[k], UI_FONT_UI, UI_TEXT, 255u);
            if (k == cur) {
                ui_circle(c, (float)(px + pw - 28), (float)(ry + 25), 9.0f, UI_ACCENT, 255u);
                ui_circle(c, (float)(px + pw - 28), (float)(ry + 25), 3.5f, 0xFFFFFFu, 255u);
            } else {
                ui_ring(c, (float)(px + pw - 28), (float)(ry + 25), 8.5f, 1.2f, 0xB0B0B5u, 255u);
            }
        }
        ui_text(c, px, oy + 176, "La distribución Latinoamérica admite ñ, tildes (´ + vocal),",
                UI_FONT_SMALL, UI_TEXT_2, 255u);
        ui_text(c, px, oy + 194, "¿ ¡ y AltGr + Q para @.", UI_FONT_SMALL, UI_TEXT_2, 255u);
    } else if (g_s.pane == P_NET) {
        ui_icon_draw(c, UI_ICON_NETWORK, px, oy + 64, 52);
        ui_text(c, px + 68, oy + 65, "Red", UI_FONT_TITLE, UI_TEXT, 255);
        ui_text(c, px + 68, oy + 96, net_description(), UI_FONT_SMALL, UI_TEXT_2, 255);
        if (net_status() != NET_ABSENT) {
            row_label(c, px, oy + 156, pw, "Adaptador", "RTL8139");
            row_label(c, px, oy + 192, pw, "IPv4 manual", "10.0.2.15");
            row_label(c, px, oy + 228, pw, "Puerta de enlace", "10.0.2.2");
            row_label(c, px, oy + 264, pw, "Servidor DNS", "10.0.2.3");
        }
        row_label(c, px, oy + 300, pw, "Wi-Fi USB", wifi_description());
    } else {
        uint64_t sec = pit_ticks_get() / 100u;
        char up[40], n[16];
        int32_t cx = px + pw / 2, ry;

        ui_icon_draw(c, UI_ICON_ABOUT, (float)(cx - 40), (float)(oy + 50), 80.0f);
        ui_text_center(c, cx, oy + 140, "MettaOS", UI_FONT_TITLE, UI_TEXT, 255u);
        ui_text_center(c, cx, oy + 170, "Metta Desktop Environment", UI_FONT_SMALL, UI_TEXT_2, 255u);

        ry = oy + 204;
        ui_rrect(c, (float)px, (float)ry, (float)pw, 150.0f, 12.0f, 0xF5F5F7u, 255u);
        row_label(c, px + 16, ry + 12, pw - 32, "Procesador", "x86_64 · QEMU");
        row_label(c, px + 16, ry + 47, pw - 32, "Almacenamiento", fs_disk_status());
        row_label(c, px + 16, ry + 82, pw - 32, "Entorno Linux",
                  ui_linux_available() ? "Disponible" : ui_linux_attached() ? "Iniciando" : "Desconectado");
        ui_itoa((int64_t)(sec / 3600u), n);
        ui_strlcpy(up, n, sizeof(up));
        ui_strlcat(up, " h ", sizeof(up));
        ui_itoa((int64_t)((sec / 60u) % 60u), n);
        ui_strlcat(up, n, sizeof(up));
        ui_strlcat(up, " min", sizeof(up));
        row_label(c, px + 16, ry + 117, pw - 32, "Tiempo activo", up);
        for (k = 1; k < 4; k++)
            ui_fill(c, px + 16, ry + k * 35 + 3, pw - 32, 1, UI_SEPARATOR, 255u);

        app_pill(c, px + pw / 2 - 150, ry + 168, 140, 32, g_s.hover_item == 0, 0);
        ui_text_center(c, px + pw / 2 - 80, ry + 175, "Reiniciar…", UI_FONT_UI, UI_TEXT, 255u);
        app_pill(c, px + pw / 2 + 10, ry + 168, 140, 32, g_s.hover_item == 1, 0);
        ui_text_center(c, px + pw / 2 + 80, ry + 175, "Apagar…", UI_FONT_UI, UI_TEXT, 255u);
    }
}

static int item_at(const ui_window_t *w, int32_t lx, int32_t ly)
{
    int32_t pw = w->w - PANE_X - 28;
    int k;

    if (g_s.pane == P_WALL) {
        if (app_hit(lx, ly, PANE_X, 46, pw, 47)) return UI_WALL_COUNT;
        for (k = 0; k < UI_WALL_COUNT; k++) {
            int32_t tx = PANE_X + (k % 2) * (TH_W + 24);
            int32_t ty = 135 + (k / 2) * (TH_H + 40);
            if (app_hit(lx, ly, tx, ty, TH_W, TH_H + 30))
                return k;
        }
    } else if (g_s.pane == P_KBD) {
        for (k = 0; k < 2; k++)
            if (app_hit(lx, ly, PANE_X, 56 + k * 50, pw, 50))
                return k;
    } else if (g_s.pane == P_ABOUT) {
        if (app_hit(lx, ly, PANE_X + pw / 2 - 150, 204 + 168, 140, 32))
            return 0;
        if (app_hit(lx, ly, PANE_X + pw / 2 + 10, 204 + 168, 140, 32))
            return 1;
    }
    return -1;
}

static void settings_mouse(ui_window_t *w, int slot, int type, int32_t lx, int32_t ly, int btn, int clicks)
{
    int hs = -1, hi;
    (void)slot;
    (void)clicks;

    if (lx >= SIDE_X && lx < SIDE_X + SIDE_W && ly >= 52 && ly < 52 + P_COUNT * 34)
        hs = (ly - 52) / 34;
    hi = item_at(w, lx, ly);
    if (type == UI_M_LEAVE) {
        hs = -1;
        hi = -1;
    }
    if (type == UI_M_HOVER || type == UI_M_LEAVE) {
        if (hs != g_s.hover_side || hi != g_s.hover_item) {
            g_s.hover_side = hs;
            g_s.hover_item = hi;
            ui_win_invalidate(w);
        }
        return;
    }
    if (type != UI_M_DOWN || btn != INPUT_BTN_LEFT)
        return;
    if (hs >= 0) {
        ui_consume();
        g_s.pane = hs;
        g_s.hover_item = -1;
        ui_win_invalidate(w);
        return;
    }
    if (hi < 0)
        return;
    ui_consume();
    if (g_s.pane == P_WALL && hi == UI_WALL_COUNT)
        g_s.temporary_glass = !ui_set_liquid_glass(!ui_liquid_glass());
    else if (g_s.pane == P_WALL)
        ui_wallpaper_set(hi);
    else if (g_s.pane == P_KBD)
        ui_kbd_layout_set(hi);
    else if (hi == 0)
        ui_reboot();
    else
        ui_shutdown();
    ui_win_invalidate(w);
}

static void settings_key(ui_window_t *w, int slot, const input_event_t *ev, const char *text)
{
    (void)slot;
    (void)text;
    if (g_s.pane == P_WALL && ev->u.key.keycode == INPUT_KEY_SPACE && !ev->u.key.modifiers) {
        g_s.temporary_glass = !ui_set_liquid_glass(!ui_liquid_glass());
        ui_win_invalidate(w);
    }
}

static int settings_tick(ui_window_t *w, int slot, uint64_t now)
{
    static uint64_t last;
    (void)slot;

    static int previous = -1;
    if (g_s.pane == P_NET && previous != (int)net_status()) {
        previous = net_status();
        ui_win_invalidate(w);
        return 1;
    }
    if (g_s.pane != P_ABOUT || now - last < 30000u)
        return 0;
    last = now;
    ui_win_invalidate(w);
    return 1;
}

static int settings_glass(ui_window_t *w, int slot, ui_rect_t *out, int max)
{
    (void)slot;
    if (max < 1)
        return 0;
    out[0] = (ui_rect_t){ SIDE_X, 8, SIDE_W, w->h - 16 };
    return 1;
}

const ui_app_t app_settings = {
    "Ajustes", UI_ICON_SETTINGS, 720, 480, 720, 480, 0xFFFFFFu, 0, 0, 0, 1,
    settings_open, settings_draw, settings_mouse, settings_key, NULL, settings_tick, settings_glass, NULL,
};

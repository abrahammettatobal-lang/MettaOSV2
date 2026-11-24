/*
 * MettaOS — Calculadora estilo macOS.
 */
#include "ui_apps_common.h"

#define BTN     50
#define GAP     10
#define PAD     16
#define GRID_Y  (UI_TITLEBAR_H + 92)

typedef struct {
    double acc;
    char   disp[24];
    int    op;        /* 0, '+', '-', '*', '/' */
    int    entering;  /* 1 = escribiendo un número nuevo */
    int    hover;
    int    pressed;
    uint64_t pressed_at;
} calc_t;

static calc_t g_c;

/* fila×4 + columna; el 0 ocupa dos columnas (17 se omite) */
static const char *const LABELS[20] = {
    "AC", "±", "%", "÷",
    "7", "8", "9", "×",
    "4", "5", "6", "−",
    "1", "2", "3", "+",
    "0", "", ",", "=",
};

static double disp_value(void)
{
    double v = 0.0, frac = 0.1;
    int i = 0, neg = 0, dot = 0;

    if (g_c.disp[0] == '-') {
        neg = 1;
        i = 1;
    }
    for (; g_c.disp[i]; i++) {
        char ch = g_c.disp[i];
        if (ch == ',' || ch == '.') {
            dot = 1;
            continue;
        }
        if (ch < '0' || ch > '9')
            continue;
        if (!dot) {
            v = v * 10.0 + (ch - '0');
        } else {
            v += (ch - '0') * frac;
            frac *= 0.1;
        }
    }
    return neg ? -v : v;
}

static void set_disp(double v)
{
    char *p;

    if (v != v || v > 1e15 || v < -1e15) {
        ui_strlcpy(g_c.disp, "Error", sizeof(g_c.disp));
        return;
    }
    ui_fmt_num(v, g_c.disp);
    for (p = g_c.disp; *p; p++)
        if (*p == '.')
            *p = ',';
}

static double apply(double a, int op, double b)
{
    switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/': return b == 0.0 ? (0.0 / 0.0) : a / b;
    default:  return b;
    }
}

static void press(int k)
{
    const char *l = LABELS[k];
    int len = (int)strlen(g_c.disp);

    if (l[0] >= '0' && l[0] <= '9') {
        if (!g_c.entering || ui_streq(g_c.disp, "0") || ui_streq(g_c.disp, "Error")) {
            g_c.disp[0] = '\0';
            len = 0;
            g_c.entering = 1;
        }
        if (len < 15) {
            g_c.disp[len] = l[0];
            g_c.disp[len + 1] = '\0';
        }
    } else if (k == 18) {
        if (!g_c.entering) {
            ui_strlcpy(g_c.disp, "0", sizeof(g_c.disp));
            g_c.entering = 1;
            len = 1;
        }
        if (!ui_str_starts(g_c.disp, ",") && len < 15) {
            int i, has = 0;
            for (i = 0; g_c.disp[i]; i++)
                if (g_c.disp[i] == ',')
                    has = 1;
            if (!has)
                ui_strlcat(g_c.disp, ",", sizeof(g_c.disp));
        }
    } else if (k == 0) {
        if (g_c.entering && !ui_streq(g_c.disp, "0")) {
            ui_strlcpy(g_c.disp, "0", sizeof(g_c.disp));
        } else {
            g_c.acc = 0.0;
            g_c.op = 0;
            ui_strlcpy(g_c.disp, "0", sizeof(g_c.disp));
        }
        g_c.entering = 0;
    } else if (k == 1) {
        set_disp(-disp_value());
    } else if (k == 2) {
        set_disp(disp_value() / 100.0);
    } else if (k == 19) {
        if (g_c.op) {
            set_disp(apply(g_c.acc, g_c.op, disp_value()));
            g_c.op = 0;
        }
        g_c.entering = 0;
    } else {
        static const int ops[20] = { [3] = '/', [7] = '*', [11] = '-', [15] = '+' };
        if (g_c.op && g_c.entering)
            set_disp(apply(g_c.acc, g_c.op, disp_value()));
        g_c.acc = disp_value();
        g_c.op = ops[k];
        g_c.entering = 0;
    }
}

static void btn_rect(int k, int32_t *x, int32_t *y, int32_t *w)
{
    int r = k / 4, col = k % 4;

    *x = PAD + col * (BTN + GAP);
    *y = GRID_Y + r * (BTN + GAP);
    *w = (k == 16) ? BTN * 2 + GAP : BTN;
}

static int btn_at(int32_t lx, int32_t ly)
{
    int k;

    for (k = 0; k < 20; k++) {
        int32_t x, y, w;
        if (k == 17)
            continue;
        btn_rect(k, &x, &y, &w);
        if (app_hit(lx, ly, x, y, w, BTN))
            return k;
    }
    return -1;
}

static void calc_open(ui_window_t *w, int slot, const char *arg)
{
    (void)slot;
    (void)arg;
    memset(&g_c, 0, sizeof(g_c));
    ui_strlcpy(g_c.disp, "0", sizeof(g_c.disp));
    g_c.hover = -1;
    g_c.pressed = -1;
    ui_strlcpy(w->title, "Calculadora", sizeof(w->title));
}

static void calc_draw(ui_window_t *w, int slot, ui_canvas_t *c, int32_t ox, int32_t oy, int focused)
{
    int k;
    int font = UI_FONT_HUGE;
    int32_t tw, right = ox + w->w - PAD - 6;
    (void)slot;
    (void)focused;

    tw = ui_text_width(g_c.disp, font);
    if (tw > w->w - 2 * PAD) {
        font = UI_FONT_LARGE;
        tw = ui_text_width(g_c.disp, font);
        if (tw > w->w - 2 * PAD) {
            font = UI_FONT_TITLE;
            tw = ui_text_width(g_c.disp, font);
        }
    }
    ui_text(c, right - tw, oy + GRID_Y - 12 - ui_font_line(font), g_c.disp, font, 0xFFFFFFu, 255u);

    for (k = 0; k < 20; k++) {
        int32_t x, y, bw;
        uint32_t bg, fg = 0xFFFFFFu;
        const char *l = LABELS[k];
        int lit;

        if (k == 17)
            continue;
        btn_rect(k, &x, &y, &bw);
        x += ox;
        y += oy;
        if (k % 4 == 3 || k == 19) {
            bg = 0x287D78u;
            if (k != 19 && g_c.op && !g_c.entering &&
                ((k == 3 && g_c.op == '/') || (k == 7 && g_c.op == '*') ||
                 (k == 11 && g_c.op == '-') || (k == 15 && g_c.op == '+'))) {
                bg = 0xFFFFFFu;
                fg = UI_ACCENT;
            }
        } else if (k < 3) {
            bg = 0x506772u;
        } else {
            bg = 0x2E454Fu;
        }
        lit = (k == g_c.pressed) ? 2 : (k == g_c.hover ? 1 : 0);
        if (lit)
            bg = ui_mix(bg, 0xFFFFFFu, lit == 2 ? 90u : 34u);
        ui_rrect(c, (float)x, (float)y, (float)bw, (float)BTN, 6.0f, bg, 255u);
        if (k == 0 && g_c.entering && !ui_streq(g_c.disp, "0"))
            l = "C";
        if (k == 16)
            ui_text(c, x + BTN / 2 - ui_text_width(l, UI_FONT_TITLE) / 2,
                    y + (BTN - ui_font_line(UI_FONT_TITLE)) / 2, l, UI_FONT_TITLE, fg, 255u);
        else
            ui_text_center(c, x + bw / 2, y + (BTN - ui_font_line(UI_FONT_TITLE)) / 2, l,
                           UI_FONT_TITLE, fg, 255u);
    }
}

static void calc_mouse(ui_window_t *w, int slot, int type, int32_t lx, int32_t ly, int btn, int clicks)
{
    int k = btn_at(lx, ly);
    (void)slot;
    (void)clicks;

    if (type == UI_M_LEAVE)
        k = -1;
    if (type == UI_M_HOVER || type == UI_M_LEAVE) {
        if (k != g_c.hover) {
            g_c.hover = k;
            ui_win_invalidate(w);
        }
        return;
    }
    if (type == UI_M_DOWN && btn == INPUT_BTN_LEFT && k >= 0) {
        ui_consume();
        press(k);
        g_c.pressed = k;
        g_c.pressed_at = ui_now();
        ui_win_invalidate(w);
    }
}

static void calc_key(ui_window_t *w, int slot, const input_event_t *ev, const char *text)
{
    int k = -1;
    char ch = text[0];
    (void)slot;

    if (ch >= '0' && ch <= '9') {
        static const int map[10] = { 16, 12, 13, 14, 8, 9, 10, 4, 5, 6 };
        k = map[ch - '0'];
    } else if (ch == '+') {
        k = 15;
    } else if (ch == '-') {
        k = 11;
    } else if (ch == '*' || ch == 'x') {
        k = 7;
    } else if (ch == '/') {
        k = 3;
    } else if (ch == '%') {
        k = 2;
    } else if (ch == '.' || ch == ',') {
        k = 18;
    } else if (ch == '=' || ev->u.key.keycode == INPUT_KEY_ENTER) {
        k = 19;
    } else if (ev->u.key.keycode == INPUT_KEY_ESC || ch == 'c') {
        k = 0;
    } else if (ev->u.key.keycode == INPUT_KEY_BACKSPACE) {
        int len = (int)strlen(g_c.disp);
        if (g_c.entering && len > 1)
            g_c.disp[len - 1] = '\0';
        else
            ui_strlcpy(g_c.disp, "0", sizeof(g_c.disp));
        ui_win_invalidate(w);
        return;
    }
    if (k < 0)
        return;
    press(k);
    g_c.pressed = k;
    g_c.pressed_at = ui_now();
    ui_win_invalidate(w);
}

static int calc_tick(ui_window_t *w, int slot, uint64_t now)
{
    (void)slot;
    if (g_c.pressed >= 0 && now - g_c.pressed_at > 110u) {
        g_c.pressed = -1;
        ui_win_invalidate(w);
        return 1;
    }
    return 0;
}

#define CALC_W (4 * BTN + 3 * GAP + 2 * PAD)
#define CALC_H (GRID_Y + 5 * BTN + 4 * GAP + PAD)

const ui_app_t app_calc = {
    "Calculadora", UI_ICON_CALC, CALC_W, CALC_H, CALC_W, CALC_H, 0x172C35u, 0, 1, 0, 1,
    calc_open, calc_draw, calc_mouse, calc_key, NULL, calc_tick, NULL, NULL,
};

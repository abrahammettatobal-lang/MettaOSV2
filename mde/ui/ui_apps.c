/*
 * MettaOS shell — registro de apps y utilidades comunes (botones, números, expresiones).
 */
#include "ui_apps_common.h"

extern const ui_app_t app_files, app_notes, app_terminal, app_calc, app_settings;
extern const ui_app_t app_linux;

const ui_app_t *const ui_apps[APP_COUNT] = {
    &app_files, &app_notes, &app_terminal, &app_calc, &app_settings, &app_linux,
};

void app_pill(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, int32_t h, int hover, int dark)
{
    float r = (float)h * 0.5f;

    if (dark) {
        ui_rrect(c, (float)x, (float)y, (float)w, (float)h, r, 0xFFFFFFu, hover ? 46u : 26u);
        ui_rrect_stroke(c, (float)x, (float)y, (float)w, (float)h, r, 1.0f, 0xFFFFFFu, 40u);
        return;
    }
    ui_shadow(c, (float)x, (float)y, (float)w, (float)h, r, 6.0f, 1.5f, 28u);
    ui_rrect(c, (float)x, (float)y, (float)w, (float)h, r, hover ? 0xF2F2F5u : 0xFFFFFFu, 255u);
    ui_rrect_stroke(c, (float)x, (float)y, (float)w, (float)h, r, 1.0f, 0x000000u, 22u);
}

void app_chevron(ui_canvas_t *c, float cx, float cy, int left, uint32_t col, uint32_t a)
{
    float d = left ? 1.0f : -1.0f;

    ui_capsule(c, cx + 2.5f * d, cy - 5.0f, cx - 2.5f * d, cy, 2.0f, col, a);
    ui_capsule(c, cx - 2.5f * d, cy, cx + 2.5f * d, cy + 5.0f, 2.0f, col, a);
}

void app_plus(ui_canvas_t *c, float cx, float cy, float s, uint32_t col, uint32_t a)
{
    ui_capsule(c, cx - s, cy, cx + s, cy, 2.0f, col, a);
    ui_capsule(c, cx, cy - s, cx, cy + s, 2.0f, col, a);
}

void app_trash_glyph(ui_canvas_t *c, float cx, float cy, uint32_t col, uint32_t a)
{
    ui_rrect_stroke(c, cx - 5.0f, cy - 4.0f, 10.0f, 11.0f, 2.0f, 1.6f, col, a);
    ui_capsule(c, cx - 7.0f, cy - 6.0f, cx + 7.0f, cy - 6.0f, 1.6f, col, a);
    ui_capsule(c, cx - 2.0f, cy - 8.0f, cx + 2.0f, cy - 8.0f, 1.6f, col, a);
}

void app_compose_glyph(ui_canvas_t *c, float cx, float cy, uint32_t col, uint32_t a)
{
    ui_rrect_stroke(c, cx - 7.0f, cy - 6.0f, 12.0f, 12.0f, 3.0f, 1.6f, col, a);
    ui_capsule(c, cx - 1.0f, cy + 1.0f, cx + 7.0f, cy - 7.0f, 2.2f, col, a);
}

void ui_fmt_num(double v, char *out)
{
    int64_t ip;
    double frac;
    int64_t f;
    char ibuf[24], fbuf[16];
    int i, neg = 0;

    if (v != v || v > 1e15 || v < -1e15) {
        ui_strlcpy(out, "Error", 16);
        return;
    }
    if (v < 0) {
        neg = 1;
        v = -v;
    }
    ip = (int64_t)v;
    frac = v - (double)ip;
    f = (int64_t)(frac * 100000000.0 + 0.5);
    if (f >= 100000000) {
        ip += 1;
        f -= 100000000;
    }
    ui_itoa(ip, ibuf);
    out[0] = '\0';
    if (neg && (ip != 0 || f != 0))
        ui_strlcat(out, "-", 32);
    ui_strlcat(out, ibuf, 32);
    if (f > 0) {
        for (i = 7; i >= 0; i--) {
            fbuf[i] = (char)('0' + (int)(f % 10));
            f /= 10;
        }
        fbuf[8] = '\0';
        for (i = 7; i > 0 && fbuf[i] == '0'; i--)
            fbuf[i] = '\0';
        ui_strlcat(out, ".", 32);
        ui_strlcat(out, fbuf, 32);
    }
}

/* -------- evaluador de expresiones (terminal: calc) -------- */

typedef struct {
    const char *s;
    int         pos;
    int         err;
} expr_t;

static double expr_sum(expr_t *e);

static void expr_ws(expr_t *e)
{
    while (e->s[e->pos] == ' ')
        e->pos++;
}

static double expr_atom(expr_t *e)
{
    double v = 0.0, scale = 1.0;
    int digits = 0, dot = 0;

    expr_ws(e);
    if (e->s[e->pos] == '(') {
        e->pos++;
        v = expr_sum(e);
        expr_ws(e);
        if (e->s[e->pos] == ')')
            e->pos++;
        else
            e->err = 1;
        return v;
    }
    if (e->s[e->pos] == '-') {
        e->pos++;
        return -expr_atom(e);
    }
    while ((e->s[e->pos] >= '0' && e->s[e->pos] <= '9') || e->s[e->pos] == '.') {
        if (e->s[e->pos] == '.') {
            dot = 1;
        } else {
            if (dot) {
                scale /= 10.0;
                v += (double)(e->s[e->pos] - '0') * scale;
            } else {
                v = v * 10.0 + (double)(e->s[e->pos] - '0');
            }
            digits++;
        }
        e->pos++;
    }
    if (!digits)
        e->err = 1;
    return v;
}

static double expr_pow(expr_t *e)
{
    double b = expr_atom(e);

    expr_ws(e);
    if (e->s[e->pos] == '^') {
        double ex, r = 1.0;
        int n, i;
        e->pos++;
        ex = expr_pow(e);
        n = (int)ex;
        for (i = 0; i < (n < 0 ? -n : n) && i < 64; i++)
            r *= b;
        return n < 0 ? 1.0 / r : r;
    }
    return b;
}

static double expr_prod(expr_t *e)
{
    double v = expr_pow(e);

    for (;;) {
        char op;
        expr_ws(e);
        op = e->s[e->pos];
        if (op != '*' && op != '/' && op != 'x' && op != '%')
            return v;
        e->pos++;
        if (op == '/') {
            double d = expr_pow(e);
            if (d == 0.0)
                e->err = 1;
            else
                v /= d;
        } else if (op == '%') {
            double d = expr_pow(e);
            if (d == 0.0)
                e->err = 1;
            else
                v = v - (double)(int64_t)(v / d) * d;
        } else {
            v *= expr_pow(e);
        }
    }
}

static double expr_sum(expr_t *e)
{
    double v = expr_prod(e);

    for (;;) {
        char op;
        expr_ws(e);
        op = e->s[e->pos];
        if (op != '+' && op != '-')
            return v;
        e->pos++;
        if (op == '+')
            v += expr_prod(e);
        else
            v -= expr_prod(e);
    }
}

int ui_eval(const char *s, double *out)
{
    expr_t e = { s, 0, 0 };
    double v = expr_sum(&e);

    expr_ws(&e);
    if (e.err || e.s[e.pos])
        return -1;
    *out = v;
    return 0;
}

void ui_memmove(char *dst, const char *src, int32_t n)
{
    int32_t i;

    if (dst < src) {
        for (i = 0; i < n; i++)
            dst[i] = src[i];
    } else {
        for (i = n - 1; i >= 0; i--)
            dst[i] = src[i];
    }
}

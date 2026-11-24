/*
 * MettaOS shell — primitivas con antialias (SDF) y Liquid Glass real:
 * blur del fondo ya pintado + saturación + tinte + refracción en el borde + rim especular.
 */
#include "ui_internal.h"

float ui_sqrtf(float v)
{
    float r;

    if (v <= 0.0f)
        return 0.0f;
    __asm__("sqrtss %1, %0" : "=x"(r) : "x"(v));
    return r;
}

#define UI_PI 3.14159265f

float ui_sinf(float x)
{
    float x2;

    while (x > UI_PI)
        x -= 2.0f * UI_PI;
    while (x < -UI_PI)
        x += 2.0f * UI_PI;
    if (x > UI_PI * 0.5f)
        x = UI_PI - x;
    else if (x < -UI_PI * 0.5f)
        x = -UI_PI - x;
    x2 = x * x;
    return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f)));
}

float ui_cosf(float x)
{
    return ui_sinf(x + UI_PI * 0.5f);
}

static inline int32_t ui_floor(float v)
{
    int32_t i = (int32_t)v;
    return (v < (float)i) ? i - 1 : i;
}

static inline int32_t ui_ceil(float v)
{
    int32_t i = (int32_t)v;
    return (v > (float)i) ? i + 1 : i;
}

void ui_rect_union(ui_rect_t *acc, const ui_rect_t *r)
{
    int32_t x0, y0, x1, y1;

    if (r->w <= 0 || r->h <= 0)
        return;
    if (acc->w <= 0 || acc->h <= 0) {
        *acc = *r;
        return;
    }
    x0 = ui_min(acc->x, r->x);
    y0 = ui_min(acc->y, r->y);
    x1 = ui_max(acc->x + acc->w, r->x + r->w);
    y1 = ui_max(acc->y + acc->h, r->y + r->h);
    acc->x = x0;
    acc->y = y0;
    acc->w = x1 - x0;
    acc->h = y1 - y0;
}

uint32_t ui_mix(uint32_t a, uint32_t b, uint32_t t)
{
    uint32_t it;
    uint32_t rb, g;

    if (t >= 255u)
        return b;
    if (t == 0u)
        return a;
    it = 255u - t;
    rb = (a & 0xFF00FFu) * it + (b & 0xFF00FFu) * t;
    rb = ((rb + 0x800080u + ((rb >> 8) & 0xFF00FFu)) >> 8) & 0xFF00FFu;
    g = (a & 0x00FF00u) * it + (b & 0x00FF00u) * t;
    g = ((g + 0x8000u + ((g >> 8) & 0xFF00u)) >> 8) & 0xFF00u;
    return rb | g;
}

static inline void ui_blend(uint32_t *d, uint32_t rgb, uint32_t a)
{
    if (a == 0u)
        return;
    *d = (a >= 255u) ? (rgb & 0xFFFFFFu) : ui_mix(*d, rgb, a);
}

void ui_canvas_clip(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, int32_t h)
{
    c->cx0 = ui_clampi(x, 0, c->w);
    c->cy0 = ui_clampi(y, 0, c->h);
    c->cx1 = ui_clampi(x + w, 0, c->w);
    c->cy1 = ui_clampi(y + h, 0, c->h);
}

/* bbox entero recortado; 0 si vacío */
static int ui_bbox(const ui_canvas_t *c, float x, float y, float w, float h,
                   int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
    *x0 = ui_max(ui_floor(x), c->cx0);
    *y0 = ui_max(ui_floor(y), c->cy0);
    *x1 = ui_min(ui_ceil(x + w), c->cx1);
    *y1 = ui_min(ui_ceil(y + h), c->cy1);
    return *x0 < *x1 && *y0 < *y1;
}

void ui_fill(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb, uint32_t a)
{
    int32_t x0 = ui_max(x, c->cx0), y0 = ui_max(y, c->cy0);
    int32_t x1 = ui_min(x + w, c->cx1), y1 = ui_min(y + h, c->cy1);
    int32_t px, py;

    if (x0 >= x1 || y0 >= y1 || a == 0u)
        return;
    for (py = y0; py < y1; py++) {
        uint32_t *row = c->px + (int64_t)py * c->stride;
        if (a >= 255u) {
            for (px = x0; px < x1; px++)
                row[px] = rgb;
        } else {
            for (px = x0; px < x1; px++)
                row[px] = ui_mix(row[px], rgb, a);
        }
    }
}

static float sd_rbox(float px, float py, float cx, float cy, float hx, float hy, float r)
{
    float qx = ui_absf(px - cx) - hx + r;
    float qy = ui_absf(py - cy) - hy + r;
    float mx = qx > 0.0f ? qx : 0.0f;
    float my = qy > 0.0f ? qy : 0.0f;
    float in = qx > qy ? qx : qy;

    if (in > 0.0f)
        in = 0.0f;
    return ui_sqrtf(mx * mx + my * my) + in - r;
}

static void rrect_impl(ui_canvas_t *c, float x, float y, float w, float h, float r,
                       uint32_t top, uint32_t bottom, uint32_t a, int grad)
{
    int32_t x0, y0, x1, y1, px, py;
    float cx, cy, hx, hy;

    if (w <= 0.0f || h <= 0.0f || a == 0u)
        return;
    if (r > w * 0.5f)
        r = w * 0.5f;
    if (r > h * 0.5f)
        r = h * 0.5f;
    if (r < 0.0f)
        r = 0.0f;
    if (!ui_bbox(c, x, y, w, h, &x0, &y0, &x1, &y1))
        return;

    cx = x + w * 0.5f;
    cy = y + h * 0.5f;
    hx = w * 0.5f;
    hy = h * 0.5f;

    for (py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t col = top;
        uint32_t *row = c->px + (int64_t)py * c->stride;
        int corner = (fy < y + r) || (fy > y + h - r);

        if (grad) {
            float t = (fy - y) / h;
            col = ui_mix(top, bottom, (uint32_t)(ui_clampf(t, 0.0f, 1.0f) * 255.0f));
        }

        if (!corner) {
            float vy = ui_clampf((fy - y < y + h - fy ? fy - y : y + h - fy) + 0.5f, 0.0f, 1.0f);
            for (px = x0; px < x1; px++) {
                float fx = (float)px + 0.5f;
                float vx = ui_clampf((fx - x < x + w - fx ? fx - x : x + w - fx) + 0.5f, 0.0f, 1.0f);
                float cov = vx * vy;

                if (cov >= 0.999f)
                    ui_blend(&row[px], col, a);
                else if (cov > 0.0f)
                    ui_blend(&row[px], col, (uint32_t)((float)a * cov));
            }
        } else {
            for (px = x0; px < x1; px++) {
                float d = sd_rbox((float)px + 0.5f, fy, cx, cy, hx, hy, r);
                float cov = ui_clampf(0.5f - d, 0.0f, 1.0f);

                if (cov > 0.0f)
                    ui_blend(&row[px], col, (uint32_t)((float)a * cov));
            }
        }
    }
}

void ui_rrect(ui_canvas_t *c, float x, float y, float w, float h, float r, uint32_t rgb, uint32_t a)
{
    rrect_impl(c, x, y, w, h, r, rgb, rgb, a, 0);
}

void ui_rrect_grad(ui_canvas_t *c, float x, float y, float w, float h, float r,
                   uint32_t top, uint32_t bottom, uint32_t a)
{
    rrect_impl(c, x, y, w, h, r, top, bottom, a, 1);
}

void ui_rrect_stroke(ui_canvas_t *c, float x, float y, float w, float h, float r,
                     float thick, uint32_t rgb, uint32_t a)
{
    int32_t x0, y0, x1, y1, px, py;
    float cx, cy, hx, hy, skip;

    if (w <= 0.0f || h <= 0.0f || a == 0u)
        return;
    if (r > w * 0.5f)
        r = w * 0.5f;
    if (r > h * 0.5f)
        r = h * 0.5f;
    if (!ui_bbox(c, x, y, w, h, &x0, &y0, &x1, &y1))
        return;
    cx = x + w * 0.5f;
    cy = y + h * 0.5f;
    hx = w * 0.5f;
    hy = h * 0.5f;
    skip = (r > thick ? r : thick) + 2.0f;

    for (py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        uint32_t *row = c->px + (int64_t)py * c->stride;
        int mid_row = (fy > y + skip) && (fy < y + h - skip);

        for (px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            float d, cov;

            if (mid_row && fx > x + thick + 2.0f && fx < x + w - thick - 2.0f) {
                int32_t nx = ui_ceil(x + w - thick - 2.0f - 0.5f) - 1;
                if (nx > px) {
                    px = nx;
                    continue;
                }
            }
            d = sd_rbox(fx, fy, cx, cy, hx, hy, r);
            cov = ui_clampf(0.5f - d, 0.0f, 1.0f) - ui_clampf(0.5f - (d + thick), 0.0f, 1.0f);
            if (cov > 0.0f)
                ui_blend(&row[px], rgb, (uint32_t)((float)a * cov));
        }
    }
}

void ui_circle(ui_canvas_t *c, float cx, float cy, float rad, uint32_t rgb, uint32_t a)
{
    int32_t x0, y0, x1, y1, px, py;

    if (rad <= 0.0f || !ui_bbox(c, cx - rad - 1.0f, cy - rad - 1.0f, rad * 2.0f + 2.0f,
                                rad * 2.0f + 2.0f, &x0, &y0, &x1, &y1))
        return;
    for (py = y0; py < y1; py++) {
        uint32_t *row = c->px + (int64_t)py * c->stride;
        float dy = (float)py + 0.5f - cy;

        for (px = x0; px < x1; px++) {
            float dx = (float)px + 0.5f - cx;
            float cov = ui_clampf(rad - ui_sqrtf(dx * dx + dy * dy) + 0.5f, 0.0f, 1.0f);

            if (cov > 0.0f)
                ui_blend(&row[px], rgb, (uint32_t)((float)a * cov));
        }
    }
}

void ui_ring(ui_canvas_t *c, float cx, float cy, float rad, float thick, uint32_t rgb, uint32_t a)
{
    ui_arc(c, cx, cy, rad, thick, -2.0f, rgb, a);
}

void ui_arc(ui_canvas_t *c, float cx, float cy, float rad, float thick,
            float half_angle_cos, uint32_t rgb, uint32_t a)
{
    int32_t x0, y0, x1, y1, px, py;
    float ext = rad + thick + 1.0f;

    if (!ui_bbox(c, cx - ext, cy - ext, ext * 2.0f, ext * 2.0f, &x0, &y0, &x1, &y1))
        return;
    for (py = y0; py < y1; py++) {
        uint32_t *row = c->px + (int64_t)py * c->stride;
        float dy = (float)py + 0.5f - cy;

        for (px = x0; px < x1; px++) {
            float dx = (float)px + 0.5f - cx;
            float len = ui_sqrtf(dx * dx + dy * dy);
            float cov = ui_clampf(thick * 0.5f - ui_absf(len - rad) + 0.5f, 0.0f, 1.0f);

            if (cov <= 0.0f)
                continue;
            if (half_angle_cos > -1.5f) {
                float up = (len > 0.01f) ? (-dy / len) : 0.0f;
                cov *= ui_clampf((up - half_angle_cos) * len + 0.5f, 0.0f, 1.0f);
            }
            if (cov > 0.0f)
                ui_blend(&row[px], rgb, (uint32_t)((float)a * cov));
        }
    }
}

void ui_capsule(ui_canvas_t *c, float ax, float ay, float bx, float by, float thick,
                uint32_t rgb, uint32_t a)
{
    int32_t x0, y0, x1, y1, px, py;
    float minx = (ax < bx ? ax : bx) - thick, maxx = (ax > bx ? ax : bx) + thick;
    float miny = (ay < by ? ay : by) - thick, maxy = (ay > by ? ay : by) + thick;
    float vx = bx - ax, vy = by - ay;
    float vv = vx * vx + vy * vy;
    float hr = thick * 0.5f;

    if (!ui_bbox(c, minx, miny, maxx - minx, maxy - miny, &x0, &y0, &x1, &y1))
        return;
    for (py = y0; py < y1; py++) {
        uint32_t *row = c->px + (int64_t)py * c->stride;
        float fy = (float)py + 0.5f;

        for (px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            float wx = fx - ax, wy = fy - ay;
            float t = vv > 0.0f ? ui_clampf((wx * vx + wy * vy) / vv, 0.0f, 1.0f) : 0.0f;
            float dx = wx - vx * t, dy = wy - vy * t;
            float cov = ui_clampf(hr - ui_sqrtf(dx * dx + dy * dy) + 0.5f, 0.0f, 1.0f);

            if (cov > 0.0f)
                ui_blend(&row[px], rgb, (uint32_t)((float)a * cov));
        }
    }
}

void ui_shadow(ui_canvas_t *c, float x, float y, float w, float h, float r,
               float blur, float offy, uint32_t a)
{
    int32_t x0, y0, x1, y1, px, py;
    float cx = x + w * 0.5f, cy = y + h * 0.5f, hx = w * 0.5f, hy = h * 0.5f;
    float scy = cy + offy;

    if (!ui_bbox(c, x - blur, y - blur + offy, w + blur * 2.0f, h + blur * 2.0f,
                 &x0, &y0, &x1, &y1))
        return;
    for (py = y0; py < y1; py++) {
        uint32_t *row = c->px + (int64_t)py * c->stride;
        float fy = (float)py + 0.5f;
        int inner_row = fy > y + r + 1.0f && fy < y + h - r - 1.0f;

        for (px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            float d, t, f;

            if (inner_row && fx > x + 1.0f && fx < x + w - 1.0f) {
                int32_t nx = ui_ceil(x + w - 1.0f - 0.5f) - 1;
                if (nx > px) {
                    px = nx;
                    continue;
                }
            }
            if (sd_rbox(fx, fy, cx, cy, hx, hy, r) < -0.5f)
                continue;
            d = sd_rbox(fx, fy, cx, scy, hx, hy, r);
            t = ui_clampf((d + blur * 0.25f) / (blur * 1.25f), 0.0f, 1.0f);
            f = 1.0f - t;
            f = f * f * (3.0f - 2.0f * f) * f;
            if (f > 0.003f)
                ui_blend(&row[px], 0x000000u, (uint32_t)((float)a * f));
        }
    }
}

/* ---------------- Liquid Glass ---------------- */

#define UI_GLASS_MAX (1024 * 520)
static uint32_t g_glass_a[UI_GLASS_MAX];
static uint32_t g_glass_b[UI_GLASS_MAX];

static void box_h(const uint32_t *src, uint32_t *dst, int32_t w, int32_t h, int32_t r)
{
    uint32_t inv = 65536u / (uint32_t)(2 * r + 1);
    int32_t x, y, i;

    for (y = 0; y < h; y++) {
        const uint32_t *row = src + (int64_t)y * w;
        uint32_t *out = dst + (int64_t)y * w;
        uint32_t sr = 0, sg = 0, sb = 0;

        for (i = -r; i <= r; i++) {
            uint32_t p = row[ui_clampi(i, 0, w - 1)];
            sr += (p >> 16) & 0xFFu;
            sg += (p >> 8) & 0xFFu;
            sb += p & 0xFFu;
        }
        for (x = 0; x < w; x++) {
            uint32_t pa, pb;

            out[x] = (((sr * inv) >> 16) << 16) | (((sg * inv) >> 16) << 8) | ((sb * inv) >> 16);
            pa = row[ui_min(x + r + 1, w - 1)];
            pb = row[ui_max(x - r, 0)];
            sr += ((pa >> 16) & 0xFFu) - ((pb >> 16) & 0xFFu);
            sg += ((pa >> 8) & 0xFFu) - ((pb >> 8) & 0xFFu);
            sb += (pa & 0xFFu) - (pb & 0xFFu);
        }
    }
}

static void box_v(const uint32_t *src, uint32_t *dst, int32_t w, int32_t h, int32_t r)
{
    uint32_t inv = 65536u / (uint32_t)(2 * r + 1);
    int32_t x, y, i;

    for (x = 0; x < w; x++) {
        uint32_t sr = 0, sg = 0, sb = 0;

        for (i = -r; i <= r; i++) {
            uint32_t p = src[(int64_t)ui_clampi(i, 0, h - 1) * w + x];
            sr += (p >> 16) & 0xFFu;
            sg += (p >> 8) & 0xFFu;
            sb += p & 0xFFu;
        }
        for (y = 0; y < h; y++) {
            uint32_t pa, pb;

            dst[(int64_t)y * w + x] =
                (((sr * inv) >> 16) << 16) | (((sg * inv) >> 16) << 8) | ((sb * inv) >> 16);
            pa = src[(int64_t)ui_min(y + r + 1, h - 1) * w + x];
            pb = src[(int64_t)ui_max(y - r, 0) * w + x];
            sr += ((pa >> 16) & 0xFFu) - ((pb >> 16) & 0xFFu);
            sg += ((pa >> 8) & 0xFFu) - ((pb >> 8) & 0xFFu);
            sb += (pa & 0xFFu) - (pb & 0xFFu);
        }
    }
}

static inline uint32_t glass_sample(const uint32_t *buf, int32_t w, int32_t h, float sx, float sy)
{
    int32_t ix, iy, ix1, iy1;
    uint32_t fx, fy, top, bot;

    sx = ui_clampf(sx, 0.0f, (float)(w - 1));
    sy = ui_clampf(sy, 0.0f, (float)(h - 1));
    ix = (int32_t)sx;
    iy = (int32_t)sy;
    ix1 = ui_min(ix + 1, w - 1);
    iy1 = ui_min(iy + 1, h - 1);
    fx = (uint32_t)((sx - (float)ix) * 255.0f);
    fy = (uint32_t)((sy - (float)iy) * 255.0f);
    top = ui_mix(buf[(int64_t)iy * w + ix], buf[(int64_t)iy * w + ix1], fx);
    bot = ui_mix(buf[(int64_t)iy1 * w + ix], buf[(int64_t)iy1 * w + ix1], fx);
    return ui_mix(top, bot, fy);
}

void ui_glass(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, int32_t h, float r,
              const ui_glass_t *g)
{
    int32_t rx0, ry0, rx1, ry1, rw, rh;
    int32_t px, py, pass;
    float cx, cy, hx, hy, band;

    if (w <= 2 || h <= 2)
        return;
    rx0 = ui_max(x, 0);
    ry0 = ui_max(y, 0);
    rx1 = ui_min(x + w, c->w);
    ry1 = ui_min(y + h, c->h);
    rw = rx1 - rx0;
    rh = ry1 - ry0;
    if (rw <= 0 || rh <= 0)
        return;
    if (ui_max(x, c->cx0) >= ui_min(x + w, c->cx1) || ui_max(y, c->cy0) >= ui_min(y + h, c->cy1))
        return;

    if ((int64_t)rw * rh > UI_GLASS_MAX) {
        ui_rrect(c, (float)x, (float)y, (float)w, (float)h, r, g->tint, 230u);
        return;
    }

    for (py = 0; py < rh; py++)
        memcpy(&g_glass_a[(int64_t)py * rw], c->px + (int64_t)(ry0 + py) * c->stride + rx0,
               (size_t)rw * 4u);

    if (g->blur > 0) {
        int32_t br = ui_min(g->blur, ui_min(rw, rh) / 2);
        for (pass = 0; pass < 3 && br > 0; pass++) {
            box_h(g_glass_a, g_glass_b, rw, rh, br);
            box_v(g_glass_b, g_glass_a, rw, rh, br);
        }
    }

    cx = (float)x + (float)w * 0.5f;
    cy = (float)y + (float)h * 0.5f;
    hx = (float)w * 0.5f;
    hy = (float)h * 0.5f;
    if (r > hx)
        r = hx;
    if (r > hy)
        r = hy;
    band = r > 6.0f ? r * 1.1f : 8.0f;

    for (py = ui_max(ry0, c->cy0); py < ui_min(ry1, c->cy1); py++) {
        uint32_t *row = c->px + (int64_t)py * c->stride;
        float fy = (float)py + 0.5f;

        for (px = ui_max(rx0, c->cx0); px < ui_min(rx1, c->cx1); px++) {
            float fx = (float)px + 0.5f;
            float d = sd_rbox(fx, fy, cx, cy, hx, hy, r);
            float cov = ui_clampf(0.5f - d, 0.0f, 1.0f);
            float inside = -d;
            float sx = fx - (float)rx0 - 0.5f, sy = fy - (float)ry0 - 0.5f;
            float nx = 0.0f, ny = 0.0f, light = 0.0f;
            uint32_t col, cr, cg, cb, l;
            int32_t ir, ig, ib;

            if (cov <= 0.0f)
                continue;

            if (inside < band) {
                float qx = ui_absf(fx - cx) - hx + r;
                float qy = ui_absf(fy - cy) - hy + r;
                if (qx > 0.0f && qy > 0.0f) {
                    float len = ui_sqrtf(qx * qx + qy * qy);
                    nx = qx / len;
                    ny = qy / len;
                } else if (qx > qy) {
                    nx = 1.0f;
                } else {
                    ny = 1.0f;
                }
                if (fx < cx)
                    nx = -nx;
                if (fy < cy)
                    ny = -ny;
                light = -(nx * 0.55f + ny * 0.83f);

                if (g->lens > 0.0f) {
                    float t = 1.0f - inside / band;
                    float amt = g->lens * t * t;
                    sx -= nx * amt;
                    sy -= ny * amt;
                }
            }

            if (inside >= band || g->lens <= 0.0f)
                col = g_glass_a[(int64_t)(py - ry0) * rw + px - rx0];
            else
                col = glass_sample(g_glass_a, rw, rh, sx, sy);

            if (g->saturation != 1.0f) {
                cr = (col >> 16) & 0xFFu;
                cg = (col >> 8) & 0xFFu;
                cb = col & 0xFFu;
                l = (cr * 77u + cg * 150u + cb * 29u) >> 8;
                ir = (int32_t)l + (int32_t)((float)((int32_t)cr - (int32_t)l) * g->saturation);
                ig = (int32_t)l + (int32_t)((float)((int32_t)cg - (int32_t)l) * g->saturation);
                ib = (int32_t)l + (int32_t)((float)((int32_t)cb - (int32_t)l) * g->saturation);
                col = UI_RGB(ui_clampi(ir, 0, 255), ui_clampi(ig, 0, 255), ui_clampi(ib, 0, 255));
            }

            col = ui_mix(col, g->tint, g->tint_a);

            if (g->rim > 0.0f && inside < band) {
                float edge = 0.0f;
                float lp = light > 0.0f ? light : 0.0f;
                float ln = light < 0.0f ? -light : 0.0f;

                if (inside < 1.8f)
                    edge = (1.0f - inside / 1.8f) * (0.30f + 0.70f * lp + 0.35f * ln);
                edge += (1.0f - inside / band) * (1.0f - inside / band) * 0.22f * lp;
                edge *= g->rim;
                if (edge > 0.0f)
                    col = ui_mix(col, g->dark ? 0xD8E2F0u : 0xFFFFFFu,
                                 (uint32_t)(ui_clampf(edge, 0.0f, 1.0f) * 255.0f));
            }

            ui_blend(&row[px], col, (uint32_t)(cov * 255.0f));
        }
    }
}

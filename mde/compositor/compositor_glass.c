/*
 * LG-T02 — Liquid Glass REAL: 3× box-blur (~gaussian), tint, specular, caché.
 */
#include "compositor_glass.h"

#include "../gfx/draw.h"

#include <stddef.h>

#define COMP_GLASS_CACHE_W       1920
#define COMP_GLASS_TOPBAR_MAX_H  40
#define COMP_GLASS_DOCK_MAX_W    640
#define COMP_GLASS_DOCK_MAX_H    120
#define COMP_GLASS_WINDOW_MAX_W  960
#define COMP_GLASS_WINDOW_MAX_H  128
#define COMP_GLASS_WORK_MAX      (640 * 120)
/* Área mínima de dirty para invalidar caché (evita cursor 16×16). */
#define COMP_GLASS_DIRTY_AREA_MIN 576u /* 24×24 */

typedef struct comp_glass_cache {
    int32_t  x;
    int32_t  y;
    int32_t  w;
    int32_t  h;
    uint8_t  valid;
} comp_glass_cache_t;

static comp_glass_cache_t g_glass_cache[COMP_GLASS_SLOT_COUNT];
static uint32_t g_glass_topbar_px[COMP_GLASS_CACHE_W * COMP_GLASS_TOPBAR_MAX_H];
static uint32_t g_glass_dock_px[COMP_GLASS_DOCK_MAX_W * COMP_GLASS_DOCK_MAX_H];
static uint32_t g_glass_window_px[COMP_GLASS_WINDOW_MAX_W * COMP_GLASS_WINDOW_MAX_H];
static uint32_t g_glass_work[COMP_GLASS_WORK_MAX];

static int comp_glass_rects_intersect(int32_t ax, int32_t ay, int32_t aw, int32_t ah,
                                      int32_t bx, int32_t by, int32_t bw, int32_t bh)
{
    int32_t ix, iy, iw, ih;

    if (aw <= 0 || ah <= 0 || bw <= 0 || bh <= 0)
        return 0;
    ix = (ax > bx) ? ax : bx;
    iy = (ay > by) ? ay : by;
    iw = ((ax + aw) < (bx + bw) ? (ax + aw) : (bx + bw)) - ix;
    ih = ((ay + ah) < (by + bh) ? (ay + ah) : (by + bh)) - iy;
    return (iw > 0 && ih > 0);
}

static uint32_t comp_glass_px_xrgb(uint32_t c)
{
    return c & 0x00FFFFFFu;
}

static uint8_t comp_glass_alpha32(uint32_t argb)
{
    return draw_argb_alpha(argb);
}

static uint32_t comp_glass_blend_xrgb(uint32_t dst, uint32_t src_rgb, uint8_t alpha)
{
    uint32_t dr, dg, db;
    uint32_t sr, sg, sb;
    uint32_t inv;
    uint32_t a;

    dst = comp_glass_px_xrgb(dst);
    src_rgb = comp_glass_px_xrgb(src_rgb);

    if (alpha >= 255u)
        return src_rgb;
    if (alpha == 0u)
        return dst;

    dr = (dst >> 16) & 0xFFu;
    dg = (dst >> 8) & 0xFFu;
    db = dst & 0xFFu;

    sr = (src_rgb >> 16) & 0xFFu;
    sg = (src_rgb >> 8) & 0xFFu;
    sb = src_rgb & 0xFFu;

    a = alpha;
    inv = 255u - a;

    dr = (sr * a + dr * inv) / 255u;
    dg = (sg * a + dg * inv) / 255u;
    db = (sb * a + db * inv) / 255u;

    return (dr << 16) | (dg << 8) | db;
}

static void comp_glass_copy_region(const draw_surface_t *s, int32_t x, int32_t y,
                                   int32_t w, int32_t h, uint32_t *dst)
{
    int32_t row, col;

    for (row = 0; row < h; row++) {
        const uint32_t *src_row =
            s->pixels + (uint64_t)(y + row) * (uint64_t)s->stride + (uint64_t)x;
        uint32_t *dst_row = dst + (uint64_t)row * (uint64_t)w;
        for (col = 0; col < w; col++)
            dst_row[col] = comp_glass_px_xrgb(src_row[col]);
    }
}

static int comp_glass_rounded_inside(int32_t lx, int32_t ly, int32_t w, int32_t h,
                                     int32_t cr)
{
    int32_t dx, dy;
    int32_t diam;
    int32_t dist2;

    if (cr <= 0)
        return 1;

    diam = cr * 2;
    if (w < diam || h < diam)
        return 1;

    if (lx >= cr && ly >= cr && lx < w - cr && ly < h - cr)
        return 1;

    if (lx < cr && ly < cr) {
        dx = cr - lx;
        dy = cr - ly;
    } else if (lx >= w - cr && ly < cr) {
        dx = lx - (w - cr - 1);
        dy = cr - ly;
    } else if (lx < cr && ly >= h - cr) {
        dx = cr - lx;
        dy = ly - (h - cr - 1);
    } else if (lx >= w - cr && ly >= h - cr) {
        dx = lx - (w - cr - 1);
        dy = ly - (h - cr - 1);
    } else {
        return 1;
    }

    dist2 = dx * dx + dy * dy;
    return dist2 <= cr * cr;
}

static void comp_glass_blit_region(draw_surface_t *s, int32_t x, int32_t y,
                                   int32_t w, int32_t h, const uint32_t *src,
                                   int32_t corner_radius)
{
    int32_t row, col;

    for (row = 0; row < h; row++) {
        uint32_t *dst_row =
            s->pixels + (uint64_t)(y + row) * (uint64_t)s->stride + (uint64_t)x;
        const uint32_t *src_row = src + (uint64_t)row * (uint64_t)w;
        for (col = 0; col < w; col++) {
            if (corner_radius > 0 &&
                !comp_glass_rounded_inside(col, row, w, h, corner_radius))
                continue;
            dst_row[col] = comp_glass_px_xrgb(src_row[col]);
        }
    }
}

/* Box blur separable O(n·r); clip de borde por réplica (edge clamp). */
static void comp_glass_box_blur_h(const uint32_t *src, uint32_t *dst, int32_t w, int32_t h,
                                  int32_t r)
{
    int32_t y, x, k;

    if (r < 1)
        r = 1;

    for (y = 0; y < h; y++) {
        const uint32_t *row = src + (uint64_t)y * (uint64_t)w;
        uint32_t *out = dst + (uint64_t)y * (uint64_t)w;

        for (x = 0; x < w; x++) {
            uint32_t sum_r = 0u, sum_g = 0u, sum_b = 0u;
            int32_t count = 0;
            int32_t d;

            for (k = x - r; k <= x + r; k++) {
                d = k;
                if (d < 0)
                    d = 0;
                if (d >= w)
                    d = w - 1;
                sum_r += (row[d] >> 16) & 0xFFu;
                sum_g += (row[d] >> 8) & 0xFFu;
                sum_b += row[d] & 0xFFu;
                count++;
            }
            out[x] = ((sum_r / (uint32_t)count) << 16) |
                     ((sum_g / (uint32_t)count) << 8) |
                     (sum_b / (uint32_t)count);
        }
    }
}

static void comp_glass_box_blur_v(const uint32_t *src, uint32_t *dst, int32_t w, int32_t h,
                                  int32_t r)
{
    int32_t x, y, k;

    if (r < 1)
        r = 1;

    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            uint32_t sum_r = 0u, sum_g = 0u, sum_b = 0u;
            int32_t count = 0;
            int32_t d;

            for (k = y - r; k <= y + r; k++) {
                d = k;
                if (d < 0)
                    d = 0;
                if (d >= h)
                    d = h - 1;
                {
                    uint32_t c =
                        src[(uint64_t)d * (uint64_t)w + (uint64_t)x];
                    sum_r += (c >> 16) & 0xFFu;
                    sum_g += (c >> 8) & 0xFFu;
                    sum_b += c & 0xFFu;
                }
                count++;
            }
            dst[(uint64_t)y * (uint64_t)w + (uint64_t)x] =
                ((sum_r / (uint32_t)count) << 16) |
                ((sum_g / (uint32_t)count) << 8) |
                (sum_b / (uint32_t)count);
        }
    }
}

/*
 * Tres radios de caja que aproximan gaussiano (Peter Vánčo / W3C filtro stack).
 * sigma ≈ blur_token / 3 en espacio de tokens MDE_GLASS_BLUR_*.
 */
static void comp_glass_box_radii(uint8_t blur_token, int32_t out_r[3])
{
    int32_t sigma;
    int32_t wlo, wmid, whi;
    int32_t t;

    sigma = (int32_t)blur_token / 3;
    if (sigma < 2)
        sigma = 2;
    if (sigma > 22)
        sigma = 22;

    /*
     * w ≈ sqrt((12·σ²)/3 + 1) en enteros: w² ≈ 4·σ² + 1
     */
    t = 4 * sigma * sigma + 1;
    wmid = 1;
    while (wmid * wmid < t)
        wmid++;
    if ((wmid & 1) == 0)
        wmid++;

    wlo = (2 * wmid + 1) / 3;
    if ((wlo & 1) == 0)
        wlo++;
    if (wlo < 3)
        wlo = 3;

    whi = wmid + (wmid - wlo);
    if ((whi & 1) == 0)
        whi++;

    out_r[0] = (wlo - 1) / 2;
    out_r[1] = (wmid - 1) / 2;
    out_r[2] = (whi - 1) / 2;
}

static void comp_glass_frost_gauss(uint32_t *buf, int32_t w, int32_t h, uint8_t blur_radius)
{
    int32_t radii[3];
    int32_t pass;

    comp_glass_box_radii(blur_radius, radii);

    for (pass = 0; pass < 3; pass++) {
        comp_glass_box_blur_h(buf, g_glass_work, w, h, radii[pass]);
        comp_glass_box_blur_v(g_glass_work, buf, w, h, radii[pass]);
    }
}

static void comp_glass_apply_tint(uint32_t *buf, int32_t w, int32_t h,
                                  const mde_glass_tokens *glass)
{
    int32_t row, col;
    uint32_t tint_rgb;
    uint8_t tint_a;

    if (!glass)
        return;

    tint_rgb = draw_color_from_theme(glass->fill_chrome);
    tint_a = comp_glass_alpha32(glass->fill_chrome);
    if (tint_a == 0u)
        tint_a = (uint8_t)0xB3u;

    for (row = 0; row < h; row++) {
        uint32_t *px = buf + (uint64_t)row * (uint64_t)w;
        for (col = 0; col < w; col++)
            px[col] = comp_glass_blend_xrgb(px[col], tint_rgb, tint_a);
    }
}

static void comp_glass_apply_specular(uint32_t *buf, int32_t w, int32_t h,
                                      const mde_glass_tokens *glass,
                                      int32_t corner_radius)
{
    int32_t row, col;
    uint32_t top_rgb;
    uint32_t edge_rgb;
    uint32_t base_rgb;
    uint8_t top_a;
    uint8_t edge_a;
    uint8_t base_a;
    uint8_t top_h;
    uint8_t border_w;
    int32_t grad_rows;

    if (!glass)
        return;

    top_rgb = draw_color_from_theme(glass->specular_top);
    top_a = comp_glass_alpha32(glass->specular_top);
    edge_rgb = draw_color_from_theme(glass->border_specular);
    edge_a = comp_glass_alpha32(glass->border_specular);
    base_rgb = draw_color_from_theme(glass->border_base);
    base_a = comp_glass_alpha32(glass->border_base);
    top_h = glass->specular_top_height;
    border_w = glass->border_width;

    if (top_h == 0u)
        top_h = 1u;
    if (border_w == 0u)
        border_w = 1u;

    grad_rows = (int32_t)top_h * 3;
    if (grad_rows < 3)
        grad_rows = 3;
    if (grad_rows > h)
        grad_rows = h;

    for (row = 0; row < h; row++) {
        uint32_t *px = buf + (uint64_t)row * (uint64_t)w;
        for (col = 0; col < w; col++) {
            uint8_t blend_a;

            if (corner_radius > 0 &&
                !comp_glass_rounded_inside(col, row, w, h, corner_radius))
                continue;

            if (row < grad_rows && top_a > 0u) {
                blend_a = (uint8_t)((uint32_t)top_a *
                                    (uint32_t)(grad_rows - row) /
                                    (uint32_t)grad_rows);
                if (blend_a > 0u)
                    px[col] = comp_glass_blend_xrgb(px[col], top_rgb, blend_a);
            }

            if ((uint32_t)col < (uint32_t)border_w ||
                (uint32_t)col >= (uint32_t)w - (uint32_t)border_w ||
                (uint32_t)row >= (uint32_t)h - (uint32_t)border_w) {
                if (edge_a > 0u)
                    px[col] = comp_glass_blend_xrgb(px[col], edge_rgb, edge_a);
                else if (base_a > 0u)
                    px[col] = comp_glass_blend_xrgb(px[col], base_rgb, base_a);
            }
        }
    }
}

static uint32_t *comp_glass_slot_buffer(comp_glass_slot_t slot, int32_t w, int32_t h)
{
    if (slot == COMP_GLASS_SLOT_TOPBAR) {
        if (w > COMP_GLASS_CACHE_W || h > COMP_GLASS_TOPBAR_MAX_H)
            return NULL;
        return g_glass_topbar_px;
    }
    if (slot == COMP_GLASS_SLOT_DOCK) {
        if (w > COMP_GLASS_DOCK_MAX_W || h > COMP_GLASS_DOCK_MAX_H)
            return NULL;
        return g_glass_dock_px;
    }
    if (slot == COMP_GLASS_SLOT_WINDOW) {
        if (w > COMP_GLASS_WINDOW_MAX_W || h > COMP_GLASS_WINDOW_MAX_H)
            return NULL;
        return g_glass_window_px;
    }
    return NULL;
}

static int comp_glass_cache_hit(comp_glass_slot_t slot, int32_t x, int32_t y,
                                int32_t w, int32_t h)
{
    const comp_glass_cache_t *c;

    if (slot >= COMP_GLASS_SLOT_COUNT)
        return 0;
    c = &g_glass_cache[slot];
    if (!c->valid)
        return 0;
    return (c->x == x && c->y == y && c->w == w && c->h == h);
}

static void comp_glass_cache_store(comp_glass_slot_t slot, int32_t x, int32_t y,
                                   int32_t w, int32_t h, const uint32_t *px)
{
    comp_glass_cache_t *c;
    uint32_t *dst;

    if (slot >= COMP_GLASS_SLOT_COUNT)
        return;
    dst = comp_glass_slot_buffer(slot, w, h);
    if (!dst)
        return;

    {
        uint64_t n = (uint64_t)w * (uint64_t)h;
        uint64_t i;

        for (i = 0; i < n; i++)
            dst[i] = px[i];
    }

    c = &g_glass_cache[slot];
    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    c->valid = 1u;
}

void comp_glass_init(void)
{
    comp_glass_invalidate_all();
}

void comp_glass_invalidate_all(void)
{
    uint32_t i;

    for (i = 0; i < COMP_GLASS_SLOT_COUNT; i++)
        g_glass_cache[i].valid = 0u;
}

void comp_glass_invalidate_slot(comp_glass_slot_t slot)
{
    if (slot < COMP_GLASS_SLOT_COUNT)
        g_glass_cache[slot].valid = 0u;
}

void comp_glass_invalidate_intersect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    uint32_t i;
    uint32_t area;

    if (w <= 0 || h <= 0)
        return;

    area = (uint32_t)w * (uint32_t)h;
    if (area < COMP_GLASS_DIRTY_AREA_MIN)
        return;

    for (i = 0; i < COMP_GLASS_SLOT_COUNT; i++) {
        const comp_glass_cache_t *c = &g_glass_cache[i];

        if (!c->valid)
            continue;
        if (comp_glass_rects_intersect(x, y, w, h, c->x, c->y, c->w, c->h))
            g_glass_cache[i].valid = 0u;
    }
}

static void comp_glass_build_panel(draw_surface_t *s, int32_t x, int32_t y,
                                   int32_t w, int32_t h,
                                   const mde_glass_tokens *glass,
                                   uint8_t blur_radius, int32_t corner_radius,
                                   uint32_t *out_buf)
{
    comp_glass_copy_region(s, x, y, w, h, out_buf);
    comp_glass_frost_gauss(out_buf, w, h, blur_radius);
    comp_glass_apply_tint(out_buf, w, h, glass);
    comp_glass_apply_specular(out_buf, w, h, glass, corner_radius);
}

void comp_glass_paint_panel(draw_surface_t *s, int32_t x, int32_t y,
                            int32_t w, int32_t h, const mde_glass_tokens *glass,
                            comp_glass_slot_t slot, uint8_t blur_radius,
                            int32_t corner_radius)
{
    uint32_t *cache_buf;
    uint32_t *build_buf;
    uint32_t stack_buf[512];
    int use_stack;

    if (!s || !s->pixels || w <= 0 || h <= 0)
        return;

    if (slot < COMP_GLASS_SLOT_COUNT &&
        comp_glass_cache_hit(slot, x, y, w, h)) {
        cache_buf = comp_glass_slot_buffer(slot, w, h);
        if (cache_buf != NULL) {
            comp_glass_blit_region(s, x, y, w, h, cache_buf, corner_radius);
            return;
        }
    }

    build_buf = comp_glass_slot_buffer(slot, w, h);
    if (build_buf == NULL) {
        use_stack = 0;
        if ((uint64_t)w * (uint64_t)h <=
            (uint64_t)(sizeof(stack_buf) / sizeof(stack_buf[0])))
            use_stack = 1;

        if (use_stack)
            build_buf = stack_buf;
        else if ((uint64_t)w * (uint64_t)h <= COMP_GLASS_WORK_MAX)
            build_buf = g_glass_work;
        else
            return;
    }

    comp_glass_build_panel(s, x, y, w, h, glass, blur_radius, corner_radius,
                           build_buf);
    comp_glass_blit_region(s, x, y, w, h, build_buf, corner_radius);

    if (slot < COMP_GLASS_SLOT_COUNT) {
        cache_buf = comp_glass_slot_buffer(slot, w, h);
        if (cache_buf != NULL)
            comp_glass_cache_store(slot, x, y, w, h, build_buf);
    }
}

void comp_glass_paint_menu(draw_surface_t *s, int32_t x, int32_t y,
                           int32_t w, int32_t h, const mde_glass_tokens *glass)
{
    uint32_t stack_buf[4096];
    uint32_t *build_buf;
    uint8_t blur;

    if (!s || !glass || w <= 0 || h <= 0)
        return;
    if ((uint64_t)w * (uint64_t)h > COMP_GLASS_WORK_MAX)
        return;

    blur = glass->blur_light;
    if ((uint64_t)w * (uint64_t)h <= (uint64_t)(sizeof(stack_buf) / sizeof(stack_buf[0])))
        build_buf = stack_buf;
    else
        build_buf = g_glass_work;

    comp_glass_build_panel(s, x, y, w, h, glass, blur, 6, build_buf);
    comp_glass_blit_region(s, x, y, w, h, build_buf, 6);
    draw_stroke_rounded_rect(s, x, y, w, h, 6,
                             draw_color_from_theme(glass->border_base),
                             (int32_t)glass->border_width);
}
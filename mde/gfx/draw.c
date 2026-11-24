/*
 * MettaOS MDE — implementación de primitivas 2D (SA-16)
 */

#include "draw.h"

static void draw_put_pixel_unclipped(draw_surface_t *s, int32_t x, int32_t y, uint32_t color)
{
    uint32_t *row;

    if (!s || !s->pixels || x < 0 || y < 0 || x >= s->width || y >= s->height)
        return;

    row = s->pixels + (int64_t)y * s->stride;
    row[x] = color;
}

static int draw_point_visible(const draw_surface_t *s, int32_t x, int32_t y)
{
    if (!s->clip_enabled)
        return x >= 0 && y >= 0 && x < s->width && y < s->height;

    return x >= s->clip.x && y >= s->clip.y &&
           x < s->clip.x + s->clip.w && y < s->clip.y + s->clip.h;
}

void draw_surface_init(draw_surface_t *s, uint32_t *pixels,
                       int32_t width, int32_t height, int32_t stride)
{
    if (!s)
        return;

    s->pixels = pixels;
    s->width = width;
    s->height = height;
    s->stride = (stride > 0) ? stride : width;
    s->clip_enabled = 0;
}

void draw_reset_clip(draw_surface_t *s)
{
    if (!s)
        return;
    s->clip_enabled = 0;
}

void draw_set_clip_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h)
{
    draw_rect_t r;

    if (!s)
        return;

    r = (draw_rect_t){ x, y, w, h };
    if (w <= 0 || h <= 0) {
        s->clip_enabled = 0;
        return;
    }

    if (!draw_clip_rect(&r, s)) {
        s->clip_enabled = 0;
        return;
    }

    s->clip = r;
    s->clip_enabled = 1;
}

int draw_clip_rect(draw_rect_t *r, const draw_surface_t *s)
{
    int32_t x2;
    int32_t y2;
    int32_t max_x;
    int32_t max_y;

    if (!r || !s || r->w <= 0 || r->h <= 0)
        return 0;

    max_x = s->width;
    max_y = s->height;

    if (r->x < 0) {
        r->w += r->x;
        r->x = 0;
    }
    if (r->y < 0) {
        r->h += r->y;
        r->y = 0;
    }

    x2 = r->x + r->w;
    y2 = r->y + r->h;
    if (x2 > max_x)
        r->w = max_x - r->x;
    if (y2 > max_y)
        r->h = max_y - r->y;

    return r->w > 0 && r->h > 0;
}

void draw_rect_union(const draw_rect_t *a, const draw_rect_t *b, draw_rect_t *out)
{
    int32_t ax2;
    int32_t ay2;
    int32_t bx2;
    int32_t by2;

    if (!a || !b || !out)
        return;

    ax2 = a->x + a->w;
    ay2 = a->y + a->h;
    bx2 = b->x + b->w;
    by2 = b->y + b->h;

    out->x = (a->x < b->x) ? a->x : b->x;
    out->y = (a->y < b->y) ? a->y : b->y;
    ax2 = (ax2 > bx2) ? ax2 : bx2;
    ay2 = (ay2 > by2) ? ay2 : by2;
    out->w = ax2 - out->x;
    out->h = ay2 - out->y;
}

void draw_pixel(draw_surface_t *s, int32_t x, int32_t y, uint32_t color)
{
    if (!draw_point_visible(s, x, y))
        return;
    draw_put_pixel_unclipped(s, x, y, color);
}

static uint32_t draw_read_pixel(const draw_surface_t *s, int32_t x, int32_t y)
{
    if (!s || !s->pixels || x < 0 || y < 0 || x >= s->width || y >= s->height)
        return 0;
    return s->pixels[(int64_t)y * s->stride + x];
}

uint32_t draw_blend_rgb(uint32_t dst_rgb, uint32_t src_rgb, uint8_t alpha)
{
    uint32_t inv;
    uint32_t out_r;
    uint32_t out_g;
    uint32_t out_b;

    if (alpha >= 255u)
        return src_rgb & 0x00FFFFFFu;
    if (alpha == 0u)
        return dst_rgb & 0x00FFFFFFu;

    inv = (uint32_t)(255u - alpha);
    out_r = (((src_rgb >> 16) & 0xFFu) * alpha + ((dst_rgb >> 16) & 0xFFu) * inv + 127u) / 255u;
    out_g = (((src_rgb >> 8) & 0xFFu) * alpha + ((dst_rgb >> 8) & 0xFFu) * inv + 127u) / 255u;
    out_b = ((src_rgb & 0xFFu) * alpha + (dst_rgb & 0xFFu) * inv + 127u) / 255u;
    return (out_r << 16) | (out_g << 8) | out_b;
}

void draw_blend_pixel(draw_surface_t *s, int32_t x, int32_t y, uint32_t argb)
{
    uint8_t a;
    uint32_t src;
    uint32_t dst;
    uint32_t blended;

    if (!draw_point_visible(s, x, y))
        return;

    a = draw_argb_alpha(argb);
    if (a == 0u)
        return;

    src = draw_argb_rgb(argb);
    dst = draw_read_pixel(s, x, y);
    blended = draw_blend_rgb(dst, src, a);
    draw_put_pixel_unclipped(s, x, y, blended);
}

void draw_fill_rect_alpha(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                          uint32_t argb)
{
    draw_rect_t clip;
    int32_t row;
    int32_t col;

    if (!s || !s->pixels || w <= 0 || h <= 0)
        return;
    if (draw_argb_alpha(argb) == 0u)
        return;

    clip = (draw_rect_t){ x, y, w, h };
    if (!draw_clip_rect(&clip, s))
        return;

    if (s->clip_enabled) {
        int32_t cx2 = s->clip.x + s->clip.w;
        int32_t cy2 = s->clip.y + s->clip.h;
        int32_t rx2 = clip.x + clip.w;
        int32_t ry2 = clip.y + clip.h;

        if (clip.x < s->clip.x)
            clip.x = s->clip.x;
        if (clip.y < s->clip.y)
            clip.y = s->clip.y;
        if (rx2 > cx2)
            clip.w = cx2 - clip.x;
        if (ry2 > cy2)
            clip.h = cy2 - clip.y;
        if (clip.w <= 0 || clip.h <= 0)
            return;
    }

    for (row = clip.y; row < clip.y + clip.h; row++) {
        for (col = clip.x; col < clip.x + clip.w; col++)
            draw_blend_pixel(s, col, row, argb);
    }
}

void draw_clear(draw_surface_t *s, uint32_t color)
{
    int32_t y;

    if (!s || !s->pixels)
        return;

    for (y = 0; y < s->height; y++) {
        int32_t x;
        uint32_t *row = s->pixels + (int64_t)y * s->stride;
        for (x = 0; x < s->width; x++)
            row[x] = color;
    }
}

void draw_fill_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                    uint32_t color)
{
    draw_rect_t clip;
    int32_t row;
    int32_t col;

    if (!s || !s->pixels || w <= 0 || h <= 0)
        return;

    clip = (draw_rect_t){ x, y, w, h };
    if (!draw_clip_rect(&clip, s))
        return;

    if (s->clip_enabled) {
        int32_t cx2 = s->clip.x + s->clip.w;
        int32_t cy2 = s->clip.y + s->clip.h;
        int32_t rx2 = clip.x + clip.w;
        int32_t ry2 = clip.y + clip.h;

        if (clip.x < s->clip.x)
            clip.x = s->clip.x;
        if (clip.y < s->clip.y)
            clip.y = s->clip.y;
        if (rx2 > cx2)
            clip.w = cx2 - clip.x;
        if (ry2 > cy2)
            clip.h = cy2 - clip.y;
        if (clip.w <= 0 || clip.h <= 0)
            return;
    }

    for (row = clip.y; row < clip.y + clip.h; row++) {
        uint32_t *dst = s->pixels + (int64_t)row * s->stride + clip.x;
        for (col = 0; col < clip.w; col++)
            dst[col] = color;
    }
}

static void draw_stroke_hsegment(draw_surface_t *s, int32_t x, int32_t y, int32_t w,
                                 uint32_t color)
{
    int32_t i;
    for (i = 0; i < w; i++)
        draw_pixel(s, x + i, y, color);
}

static void draw_stroke_vsegment(draw_surface_t *s, int32_t x, int32_t y, int32_t h,
                                 uint32_t color)
{
    int32_t i;
    for (i = 0; i < h; i++)
        draw_pixel(s, x, y + i, color);
}

void draw_hline(draw_surface_t *s, int32_t x, int32_t y, int32_t w, uint32_t color)
{
    draw_rect_t clip;

    if (!s || w <= 0)
        return;

    clip = (draw_rect_t){ x, y, w, 1 };
    if (!draw_clip_rect(&clip, s))
        return;
    draw_stroke_hsegment(s, clip.x, clip.y, clip.w, color);
}

void draw_vline(draw_surface_t *s, int32_t x, int32_t y, int32_t h, uint32_t color)
{
    draw_rect_t clip;

    if (!s || h <= 0)
        return;

    clip = (draw_rect_t){ x, y, 1, h };
    if (!draw_clip_rect(&clip, s))
        return;
    draw_stroke_vsegment(s, clip.x, clip.y, clip.h, color);
}

void draw_stroke_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                      uint32_t color, int32_t thickness)
{
    int32_t t;

    if (!s || w <= 0 || h <= 0 || thickness <= 0)
        return;

    for (t = 0; t < thickness; t++) {
        draw_hline(s, x, y + t, w, color);
        draw_hline(s, x, y + h - 1 - t, w, color);
        draw_vline(s, x + t, y, h, color);
        draw_vline(s, x + w - 1 - t, y, h, color);
    }
}

void draw_line(draw_surface_t *s, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
               uint32_t color)
{
    int32_t dx;
    int32_t dy;
    int32_t sx;
    int32_t sy;
    int32_t err;
    int32_t e2;

    if (!s)
        return;

    dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = dx - dy;

    for (;;) {
        draw_pixel(s, x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static int32_t draw_clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static uint8_t draw_rounded_rect_cover(int32_t px, int32_t py, int32_t x, int32_t y,
                                       int32_t w, int32_t h, int32_t radius)
{
    int32_t left;
    int32_t right;
    int32_t top;
    int32_t bottom;
    int32_t inner_left;
    int32_t inner_right;
    int32_t inner_top;
    int32_t inner_bottom;
    int32_t cx;
    int32_t cy;
    int32_t dx;
    int32_t dy;
    int32_t dist_sq;
    int32_t r_out;
    int32_t r_in;

    if (w <= 0 || h <= 0)
        return 0;

    if (radius <= 0)
        return 255;

    if (radius * 2 > w)
        radius = w / 2;
    if (radius * 2 > h)
        radius = h / 2;

    left = x;
    top = y;
    right = x + w - 1;
    bottom = y + h - 1;

    if (px < left - 1 || px > right + 1 || py < top - 1 || py > bottom + 1)
        return 0;

    inner_left = left + radius;
    inner_right = right - radius;
    inner_top = top + radius;
    inner_bottom = bottom - radius;

    if (px >= inner_left && px <= inner_right && py >= inner_top && py <= inner_bottom)
        return 255;

    if (px >= left + 1 && px <= right - 1) {
        if (py == top)
            return 128;
        if (py == bottom)
            return 128;
    }
    if (py >= top + 1 && py <= bottom - 1) {
        if (px == left)
            return 128;
        if (px == right)
            return 128;
    }

    if (px < inner_left && py < inner_top) {
        cx = inner_left;
        cy = inner_top;
    } else if (px > inner_right && py < inner_top) {
        cx = inner_right;
        cy = inner_top;
    } else if (px < inner_left && py > inner_bottom) {
        cx = inner_left;
        cy = inner_bottom;
    } else if (px > inner_right && py > inner_bottom) {
        cx = inner_right;
        cy = inner_bottom;
    } else
        return 255;

    dx = px - cx;
    dy = py - cy;
    dist_sq = dx * dx + dy * dy;
    r_out = radius;
    r_in = radius - 1;
    if (r_in < 0)
        r_in = 0;

    if (dist_sq <= r_in * r_in)
        return 255;
    if (dist_sq >= r_out * r_out)
        return 0;

    if (r_out == r_in)
        return 128;

    return (uint8_t)((r_out * r_out - dist_sq) * 255 / ((r_out * r_out - r_in * r_in) + 1));
}

static uint8_t draw_rounded_ring_cover(int32_t px, int32_t py, int32_t x, int32_t y,
                                       int32_t w, int32_t h, int32_t radius, int32_t thick)
{
    uint8_t outer;
    uint8_t inner;
    int32_t iw;
    int32_t ih;
    int32_t ir;

    if (thick <= 0)
        return 0;

    outer = draw_rounded_rect_cover(px, py, x, y, w, h, radius);
    if (outer == 0)
        return 0;

    iw = w - 2 * thick;
    ih = h - 2 * thick;
    if (iw <= 0 || ih <= 0)
        return outer;

    ir = radius - thick;
    if (ir < 0)
        ir = 0;

    inner = draw_rounded_rect_cover(px, py, x + thick, y + thick, iw, ih, ir);
    if (outer <= inner)
        return 0;
    return (uint8_t)(outer - inner);
}

void draw_fill_rounded_rect_alpha(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                                  int32_t radius, uint32_t argb)
{
    draw_rect_t clip;
    int32_t row;
    int32_t col;
    uint8_t base_a;
    uint32_t rgb;
    int32_t rad;

    if (!s || !s->pixels || w <= 0 || h <= 0)
        return;

    base_a = draw_argb_alpha(argb);
    if (base_a == 0u)
        return;

    rgb = draw_argb_rgb(argb);
    rad = radius;

    if (rad <= 0) {
        draw_fill_rect_alpha(s, x, y, w, h, argb);
        return;
    }

    if (rad * 2 > w)
        rad = w / 2;
    if (rad * 2 > h)
        rad = h / 2;

    clip = (draw_rect_t){ x, y, w, h };
    if (!draw_clip_rect(&clip, s))
        return;

    clip.x -= 1;
    clip.y -= 1;
    clip.w += 2;
    clip.h += 2;
    if (!draw_clip_rect(&clip, s))
        return;

    if (s->clip_enabled) {
        int32_t cx2 = s->clip.x + s->clip.w;
        int32_t cy2 = s->clip.y + s->clip.h;
        int32_t rx2 = clip.x + clip.w;
        int32_t ry2 = clip.y + clip.h;

        if (clip.x < s->clip.x)
            clip.x = s->clip.x;
        if (clip.y < s->clip.y)
            clip.y = s->clip.y;
        if (rx2 > cx2)
            clip.w = cx2 - clip.x;
        if (ry2 > cy2)
            clip.h = cy2 - clip.y;
        if (clip.w <= 0 || clip.h <= 0)
            return;
    }

    for (row = clip.y; row < clip.y + clip.h; row++) {
        for (col = clip.x; col < clip.x + clip.w; col++) {
            uint8_t cover;
            uint32_t blended_a;

            cover = draw_rounded_rect_cover(col, row, x, y, w, h, rad);
            if (cover == 0)
                continue;
            blended_a = (uint8_t)(((uint32_t)base_a * cover + 127u) / 255u);
            if (blended_a == 0)
                continue;
            draw_blend_pixel(s, col, row,
                             ((uint32_t)blended_a << 24) | (rgb & 0x00FFFFFFu));
        }
    }
}

void draw_fill_rounded_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                            int32_t radius, uint32_t color)
{
    if (!s || w <= 0 || h <= 0)
        return;

    if (radius <= 0) {
        draw_fill_rect(s, x, y, w, h, color);
        return;
    }

    draw_fill_rounded_rect_alpha(s, x, y, w, h, radius,
                                 0xFF000000u | (color & 0x00FFFFFFu));
}

void draw_stroke_rounded_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                              int32_t radius, uint32_t color, int32_t thickness)
{
    if (!s || w <= 0 || h <= 0)
        return;

    if (radius <= 0 || thickness <= 0) {
        draw_stroke_rect(s, x, y, w, h, color, thickness);
        return;
    }

    if (radius * 2 > w)
        radius = w / 2;
    if (radius * 2 > h)
        radius = h / 2;

    draw_hline(s, x + radius, y, w - 2 * radius, color);
    draw_hline(s, x + radius, y + h - 1, w - 2 * radius, color);
    draw_vline(s, x, y + radius, h - 2 * radius, color);
    draw_vline(s, x + w - 1, y + radius, h - 2 * radius, color);

    /* Arcos aproximados con líneas radiales en esquinas (suficiente para chrome MDE). */
    draw_line(s, x + radius, y, x, y + radius, color);
    draw_line(s, x + w - radius - 1, y, x + w - 1, y + radius, color);
    draw_line(s, x, y + h - radius - 1, x + radius, y + h - 1, color);
    draw_line(s, x + w - radius - 1, y + h - 1, x + w - 1, y + h - radius - 1, color);

    if (thickness > 1)
        draw_stroke_rect(s, x + 1, y + 1, w - 2, h - 2, color, thickness - 1);
}

void draw_blit(draw_surface_t *dst, int32_t dst_x, int32_t dst_y,
               const uint32_t *src, int32_t src_w, int32_t src_h, int32_t src_stride)
{
    draw_rect_t clip;
    int32_t row;
    int32_t col;

    if (!dst || !dst->pixels || !src || src_w <= 0 || src_h <= 0)
        return;

    if (src_stride <= 0)
        src_stride = src_w;

    clip = (draw_rect_t){ dst_x, dst_y, src_w, src_h };
    if (!draw_clip_rect(&clip, dst))
        return;

    if (dst->clip_enabled) {
        int32_t cx2 = dst->clip.x + dst->clip.w;
        int32_t cy2 = dst->clip.y + dst->clip.h;
        int32_t rx2 = clip.x + clip.w;
        int32_t ry2 = clip.y + clip.h;

        if (clip.x < dst->clip.x)
            clip.x = dst->clip.x;
        if (clip.y < dst->clip.y)
            clip.y = dst->clip.y;
        if (rx2 > cx2)
            clip.w = cx2 - clip.x;
        if (ry2 > cy2)
            clip.h = cy2 - clip.y;
        if (clip.w <= 0 || clip.h <= 0)
            return;
    }

    for (row = 0; row < clip.h; row++) {
        int32_t sy = clip.y + row;
        int32_t src_y = sy - dst_y;
        const uint32_t *src_row = src + (int64_t)src_y * src_stride;

        for (col = 0; col < clip.w; col++) {
            int32_t sx = clip.x + col;
            int32_t src_x = sx - dst_x;
            draw_put_pixel_unclipped(dst, sx, sy, src_row[src_x]);
        }
    }
}

#define DRAW_FROST_TILE      56
#define DRAW_FROST_PATCH_PITCH (DRAW_FROST_TILE + 4)
#define DRAW_FROST_PATCH_MAX   (DRAW_FROST_PATCH_PITCH * DRAW_FROST_PATCH_PITCH)

static uint32_t draw_box_sample_avg(const uint32_t *src, int32_t spitch, int32_t sw, int32_t sh,
                                    int32_t cx, int32_t cy, int32_t k)
{
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    int32_t yy;
    int32_t xx;
    uint32_t r_sum;
    uint32_t g_sum;
    uint32_t b_sum;
    int32_t count;

    x0 = cx - k;
    y0 = cy - k;
    x1 = cx + k;
    y1 = cy + k;

    r_sum = 0;
    g_sum = 0;
    b_sum = 0;
    count = 0;

    for (yy = y0; yy <= y1; yy++) {
        int32_t sy = draw_clamp_i32(yy, 0, sh - 1);
        for (xx = x0; xx <= x1; xx++) {
            int32_t sx = draw_clamp_i32(xx, 0, sw - 1);
            uint32_t c = src[(int64_t)sy * spitch + sx];
            r_sum += (c >> 16) & 0xFFu;
            g_sum += (c >> 8) & 0xFFu;
            b_sum += c & 0xFFu;
            count++;
        }
    }

    if (count <= 0)
        return 0;

    return (((r_sum / (uint32_t)count) & 0xFFu) << 16) |
           (((g_sum / (uint32_t)count) & 0xFFu) << 8) |
           ((b_sum / (uint32_t)count) & 0xFFu);
}

static void draw_blur_patch_box(uint32_t *dst, const uint32_t *src, int32_t pw, int32_t ph,
                                int32_t spitch, int32_t k)
{
    int32_t y;
    int32_t x;

    for (y = 0; y < ph; y++) {
        for (x = 0; x < pw; x++)
            dst[(int64_t)y * pw + x] = draw_box_sample_avg(src, spitch, pw, ph, x, y, k);
    }
}

static void draw_frosted_rect_blur(draw_surface_t *s, const draw_rect_t *region, int32_t k)
{
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    int32_t ty;
    int32_t tx;

    if (!s || !region || k <= 0)
        return;

    x0 = region->x;
    y0 = region->y;
    x1 = region->x + region->w;
    y1 = region->y + region->h;

    for (ty = y0; ty < y1; ty += DRAW_FROST_TILE) {
        int32_t th = y1 - ty;
        if (th > DRAW_FROST_TILE)
            th = DRAW_FROST_TILE;

        for (tx = x0; tx < x1; tx += DRAW_FROST_TILE) {
            int32_t tw = x1 - tx;
            int32_t patch_h;
            int32_t patch_w;
            int32_t halo;
            int32_t sy;
            int32_t sx;
            uint32_t patch[DRAW_FROST_PATCH_MAX];
            uint32_t blurred[DRAW_FROST_PATCH_MAX];
            int32_t dy;
            int32_t dx;

            if (tw > DRAW_FROST_TILE)
                tw = DRAW_FROST_TILE;

            halo = k;
            patch_w = tw + 2 * halo;
            patch_h = th + 2 * halo;
            if (patch_w > DRAW_FROST_PATCH_PITCH || patch_h > DRAW_FROST_PATCH_PITCH)
                continue;

            for (sy = 0; sy < patch_h; sy++) {
                int32_t src_y = ty - halo + sy;
                for (sx = 0; sx < patch_w; sx++) {
                    int32_t src_x = tx - halo + sx;
                    patch[(int64_t)sy * DRAW_FROST_PATCH_PITCH + sx] =
                        draw_read_pixel(s, src_x, src_y);
                }
            }

            draw_blur_patch_box(blurred, patch, patch_w, patch_h, DRAW_FROST_PATCH_PITCH, k);

            for (dy = 0; dy < th; dy++) {
                for (dx = 0; dx < tw; dx++) {
                    int32_t px = tx + dx;
                    int32_t py = ty + dy;
                    uint32_t c;

                    if (!draw_point_visible(s, px, py))
                        continue;
                    c = blurred[(int64_t)(dy + halo) * DRAW_FROST_PATCH_PITCH + (dx + halo)];
                    draw_put_pixel_unclipped(s, px, py, c);
                }
            }
        }
    }
}

void draw_frosted_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                       int32_t radius, int32_t kernel_size, uint32_t tint_argb)
{
    draw_rect_t clip;
    int32_t k;
    int32_t row;
    int32_t col;
    int32_t rad;
    uint8_t tint_a;

    if (!s || !s->pixels || w <= 0 || h <= 0)
        return;

    if (kernel_size == 3)
        k = 1;
    else if (kernel_size == 5)
        k = 2;
    else
        k = 1;

    clip = (draw_rect_t){ x, y, w, h };
    if (!draw_clip_rect(&clip, s))
        return;

    if (s->clip_enabled) {
        int32_t cx2 = s->clip.x + s->clip.w;
        int32_t cy2 = s->clip.y + s->clip.h;
        int32_t rx2 = clip.x + clip.w;
        int32_t ry2 = clip.y + clip.h;

        if (clip.x < s->clip.x)
            clip.x = s->clip.x;
        if (clip.y < s->clip.y)
            clip.y = s->clip.y;
        if (rx2 > cx2)
            clip.w = cx2 - clip.x;
        if (ry2 > cy2)
            clip.h = cy2 - clip.y;
        if (clip.w <= 0 || clip.h <= 0)
            return;
    }

    draw_frosted_rect_blur(s, &clip, k);

    tint_a = draw_argb_alpha(tint_argb);
    if (tint_a == 0u)
        return;

    rad = radius;
    if (rad * 2 > w)
        rad = w / 2;
    if (rad * 2 > h)
        rad = h / 2;

    for (row = clip.y; row < clip.y + clip.h; row++) {
        for (col = clip.x; col < clip.x + clip.w; col++) {
            uint8_t cover;
            uint32_t pixel_argb;

            cover = draw_rounded_rect_cover(col, row, x, y, w, h, rad);
            if (cover == 0)
                continue;

            pixel_argb = ((uint32_t)((tint_a * cover + 127u) / 255u) << 24) |
                         (tint_argb & 0x00FFFFFFu);
            draw_blend_pixel(s, col, row, pixel_argb);
        }
    }
}

static void draw_stroke_rounded_rect_alpha(draw_surface_t *s, int32_t x, int32_t y, int32_t w,
                                           int32_t h, int32_t radius, uint32_t border_argb,
                                           int32_t thickness)
{
    draw_rect_t clip;
    int32_t row;
    int32_t col;
    int32_t rad;
    uint8_t base_a;
    uint32_t rgb;

    if (!s || w <= 0 || h <= 0 || thickness <= 0)
        return;

    base_a = draw_argb_alpha(border_argb);
    if (base_a == 0u)
        return;

    rgb = draw_argb_rgb(border_argb);
    rad = radius;
    if (rad * 2 > w)
        rad = w / 2;
    if (rad * 2 > h)
        rad = h / 2;

    clip = (draw_rect_t){ x, y, w, h };
    if (!draw_clip_rect(&clip, s))
        return;

    clip.x -= 1;
    clip.y -= 1;
    clip.w += 2;
    clip.h += 2;
    if (!draw_clip_rect(&clip, s))
        return;

    for (row = clip.y; row < clip.y + clip.h; row++) {
        for (col = clip.x; col < clip.x + clip.w; col++) {
            uint8_t ring;
            uint8_t a;

            ring = draw_rounded_ring_cover(col, row, x, y, w, h, rad, thickness);
            if (ring == 0)
                continue;
            a = (uint8_t)(((uint32_t)base_a * ring + 127u) / 255u);
            if (a == 0)
                continue;
            draw_blend_pixel(s, col, row, ((uint32_t)a << 24) | (rgb & 0x00FFFFFFu));
        }
    }
}

void draw_specular_edge(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                        int32_t radius, uint32_t specular_argb, int32_t height_px)
{
    draw_rect_t band;
    int32_t row;
    int32_t col;
    int32_t rad;
    int32_t h_px;
    uint8_t base_a;
    uint32_t rgb;

    if (!s || w <= 0 || h <= 0)
        return;

    h_px = height_px;
    if (h_px <= 0)
        h_px = 1;
    if (h_px > 2)
        h_px = 2;
    if (h_px > h)
        h_px = h;

    base_a = draw_argb_alpha(specular_argb);
    if (base_a == 0u)
        return;

    rgb = draw_argb_rgb(specular_argb);
    rad = radius;
    if (rad * 2 > w)
        rad = w / 2;

    band = (draw_rect_t){ x, y, w, h_px };
    if (!draw_clip_rect(&band, s))
        return;

    for (row = band.y; row < band.y + band.h; row++) {
        for (col = band.x; col < band.x + band.w; col++) {
            uint8_t cover;
            uint8_t a;

            cover = draw_rounded_rect_cover(col, row, x, y, w, h, rad);
            if (cover == 0)
                continue;
            if (row != y && cover < 200)
                continue;
            a = (uint8_t)(((uint32_t)base_a * cover + 127u) / 255u);
            draw_blend_pixel(s, col, row, ((uint32_t)a << 24) | (rgb & 0x00FFFFFFu));
        }
    }
}

void draw_drop_shadow(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                      int32_t radius)
{
    static const uint8_t layers_alpha[3] = { 10, 18, 28 };
    static const int32_t layers_off_y[3] = { 1, 4, 12 };
    static const int32_t layers_spread[3] = { 2, 4, 8 };
    int32_t i;
    int32_t rad;

    if (!s || w <= 0 || h <= 0)
        return;

    rad = radius;
    if (rad * 2 > w)
        rad = w / 2;
    if (rad * 2 > h)
        rad = h / 2;

    for (i = 0; i < 3; i++) {
        int32_t spread = layers_spread[i];
        int32_t off_y = layers_off_y[i];
        uint32_t shadow_argb;
        int32_t sx;
        int32_t sy;
        int32_t sw;
        int32_t sh;
        int32_t sr;

        sx = x - spread;
        sy = y + off_y;
        sw = w + 2 * spread;
        sh = h + off_y + spread;
        sr = rad + spread;
        shadow_argb = ((uint32_t)layers_alpha[i] << 24);
        draw_fill_rounded_rect_alpha(s, sx, sy, sw, sh, sr, shadow_argb);
    }
}

void draw_glass_panel(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                      int32_t radius, uint32_t fill_argb, uint32_t border_argb)
{
    int32_t blur_k;
    uint8_t frosted_alpha;
    int32_t spec_h;

    if (!s || w <= 0 || h <= 0)
        return;

    draw_drop_shadow(s, x, y, w, h, radius);

    frosted_alpha = draw_argb_alpha(fill_argb);
    blur_k = (frosted_alpha > 200u) ? 5 : 3;
    draw_frosted_rect(s, x, y, w, h, radius, blur_k, fill_argb);

    draw_stroke_rounded_rect_alpha(s, x, y, w, h, radius, border_argb, 1);

    spec_h = (radius >= 10) ? 2 : 1;
    draw_specular_edge(s, x, y, w, h, radius, 0x9900FFFFu, spec_h);
}
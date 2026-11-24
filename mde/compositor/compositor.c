/*
 * MettaOS MDE — compositor: doble buffer + damage + present parcial (SA-15)
 */
#include "compositor.h"
#include "compositor_glass.h"

#include <stddef.h>

static void comp_log_count(compositor_t *c)
{
    char buf[80];
    int n;

    if (!c || !c->cfg.log_dirty_counts || !c->cfg.log_fn)
        return;

    n = 0;
    buf[0] = '\0';
    /* snprintf freestanding no garantizado; mensaje fijo con dígitos manuales */
    (void)n;
    if (c->stats.full_present_count > 0 &&
        c->stats.last_present_count == 0) {
        c->cfg.log_fn("[comp] present full_frame");
        return;
    }
    if (c->stats.last_present_count <= 9) {
        buf[0] = '[';
        buf[1] = 'c';
        buf[2] = 'o';
        buf[3] = 'm';
        buf[4] = 'p';
        buf[5] = ']';
        buf[6] = ' ';
        buf[7] = 'r';
        buf[8] = '=';
        buf[9] = (char)('0' + (char)c->stats.last_present_count);
        buf[10] = ' ';
        buf[11] = 'd';
        buf[12] = '=';
        buf[13] = (char)('0' + (char)(c->stats.last_damage_count % 10u));
        buf[14] = '\0';
        c->cfg.log_fn(buf);
    } else {
        c->cfg.log_fn("[comp] present partial");
    }
}

static void comp_back_fill_rect(uint32_t *back, int32_t stride,
                                const fb_rect_t *r, uint32_t color)
{
    int32_t row, col;

    if (!back || !r || r->w <= 0 || r->h <= 0)
        return;

    for (row = 0; row < r->h; row++) {
        uint32_t *dst = back + (uint64_t)(r->y + row) * (uint64_t)stride + (uint64_t)r->x;
        for (col = 0; col < r->w; col++)
            dst[col] = color;
    }
}

static void comp_back_blit(uint32_t *back, int32_t stride,
                           int32_t dst_x, int32_t dst_y,
                           int32_t w, int32_t h,
                           const uint32_t *src, int32_t src_stride)
{
    int32_t row, col;

    if (!back || !src || w <= 0 || h <= 0)
        return;
    if (src_stride <= 0)
        src_stride = w;

    for (row = 0; row < h; row++) {
        uint32_t *dst_row = back + (uint64_t)(dst_y + row) * (uint64_t)stride + (uint64_t)dst_x;
        const uint32_t *src_row = src + (uint64_t)row * (uint64_t)src_stride;
        for (col = 0; col < w; col++)
            dst_row[col] = src_row[col];
    }
}

static void comp_cursor_rect(compositor_t *c, int32_t x, int32_t y, fb_rect_t *out)
{
    if (!out)
        return;
    out->x = x;
    out->y = y;
    out->w = (int32_t)c->cfg.cursor_w;
    out->h = (int32_t)c->cfg.cursor_h;
    if (c->fb)
        (void)fb_clip_rect(out, c->fb);
}

int compositor_init(compositor_t *c, const framebuffer_t *fb,
                    uint32_t *back_pixels, const compositor_config_t *cfg)
{
    if (!c || !fb || !back_pixels || !fb->ready)
        return -1;

    c->fb = fb;
    c->back = back_pixels;
    c->width = fb->width;
    c->height = fb->height;
    c->stride_px = (int32_t)fb->width;
    c->ready = 0;

    if (cfg) {
        c->cfg = *cfg;
    } else {
        compositor_config_t d = { 16, 16, 0, NULL };
        c->cfg = d;
    }
    if (c->cfg.cursor_w == 0)
        c->cfg.cursor_w = 16;
    if (c->cfg.cursor_h == 0)
        c->cfg.cursor_h = 16;

    comp_damage_init(&c->damage);
    comp_damage_init(&c->present_damage);
    comp_damage_mark_full(&c->damage);
    comp_damage_mark_full(&c->present_damage);

    c->cursor_visible = 0;
    c->cursor_x = 0;
    c->cursor_y = 0;
    c->cursor_rect = (fb_rect_t){ 0, 0, 0, 0 };
    c->cursor_prev = (fb_rect_t){ 0, 0, 0, 0 };

    c->stats.frames = 0;
    c->stats.present_rects_total = 0;
    c->stats.last_damage_count = 1;
    c->stats.last_present_count = 0;
    c->stats.full_present_count = 0;

    c->pointer_x           = 0;
    c->pointer_y           = 0;
    c->last_hover_icon_id  = -1;
    c->ctx_last_hover      = -1;
    c->chrome_seeded       = 0;
    c->icon_count          = 0;
    c->last_frame_ms       = 0;

    c->ready = 1;
    return 0;
}

void compositor_shutdown(compositor_t *c)
{
    if (!c)
        return;
    c->ready = 0;
    c->fb = NULL;
    c->back = NULL;
    comp_glass_invalidate_all();
    comp_damage_reset(&c->damage);
    comp_damage_reset(&c->present_damage);
}

void compositor_begin_frame(compositor_t *c, int invalidate_all)
{
    if (!c || !c->ready)
        return;
    if (invalidate_all) {
        comp_glass_invalidate_all();
        comp_damage_mark_full(&c->damage);
    }
}

void compositor_damage_rect(compositor_t *c, int32_t x, int32_t y,
                            int32_t w, int32_t h)
{
    if (!c || !c->ready)
        return;
    comp_damage_mark(&c->damage, c->fb, x, y, w, h);
}

void compositor_damage_full(compositor_t *c)
{
    if (!c || !c->ready)
        return;
    comp_glass_invalidate_all();
    comp_damage_mark_full(&c->damage);
}

uint32_t *compositor_back_buffer(compositor_t *c)
{
    return (c && c->ready) ? c->back : NULL;
}

int32_t compositor_stride(compositor_t *c)
{
    return (c && c->ready) ? c->stride_px : 0;
}

const compositor_stats_t *compositor_stats(const compositor_t *c)
{
    return c ? &c->stats : NULL;
}

void compositor_render(compositor_t *c, compositor_draw_region_fn draw, void *ctx)
{
    uint32_t i, n;

    if (!c || !c->ready)
        return;

    comp_damage_merge(&c->damage);

    if (comp_damage_is_full(&c->damage)) {
        fb_rect_t full;
        if (draw) {
            full.x = 0;
            full.y = 0;
            full.w = (int32_t)c->width;
            full.h = (int32_t)c->height;
            draw(ctx, full.x, full.y, full.w, full.h, c->back, c->stride_px);
        }
    } else if (draw) {
        n = comp_damage_count(&c->damage);
        for (i = 0; i < n; i++) {
            const fb_rect_t *r = &c->damage.rects[i];
            draw(ctx, r->x, r->y, r->w, r->h, c->back, c->stride_px);
        }
    }

    /* Snapshot para present: no borrar damage hasta después del blit. */
    c->present_damage = c->damage;
    c->stats.last_damage_count = comp_damage_is_full(&c->present_damage)
        ? 0u
        : comp_damage_count(&c->present_damage);
    if (comp_damage_is_full(&c->present_damage))
        c->stats.last_damage_count = 1u;

    comp_damage_reset(&c->damage);
}

void compositor_present(compositor_t *c)
{
    uint32_t i, n;
    fb_rect_t full;
    fb_dirty_t *fb_dmg;

    if (!c || !c->ready || !c->fb)
        return;

    c->stats.frames++;
    c->stats.last_present_count = 0;

    if (comp_damage_is_full(&c->present_damage)) {
        full.x = 0;
        full.y = 0;
        full.w = (int32_t)c->width;
        full.h = (int32_t)c->height;
        fb_blit(full.x, full.y, full.w, full.h, c->back, c->stride_px);
        c->stats.full_present_count++;
        c->stats.last_present_count = 0;
    } else {
        comp_damage_merge(&c->present_damage);
        n = comp_damage_count(&c->present_damage);
        for (i = 0; i < n; i++) {
            const fb_rect_t *r = &c->present_damage.rects[i];
            fb_blit(r->x, r->y, r->w, r->h,
                    c->back + (int64_t)r->y * c->stride_px + r->x, c->stride_px);
            c->stats.last_present_count++;
        }
        c->stats.present_rects_total += c->stats.last_present_count;
    }

    fb_dmg = fb_dirty_get();
    (void)fb_dmg; /* el caller puede inspeccionar antes de fb_present */

    fb_present();

    comp_damage_reset(&c->present_damage);
    comp_log_count(c);
}

void compositor_cursor_move(compositor_t *c, int32_t x, int32_t y)
{
    fb_rect_t new_r;

    if (!c || !c->ready)
        return;

    if (c->cursor_visible)
        comp_damage_mark_rect(&c->damage, c->fb, &c->cursor_prev);

    c->cursor_x = x;
    c->cursor_y = y;
    comp_cursor_rect(c, x, y, &new_r);
    c->cursor_rect = new_r;
    c->cursor_prev = new_r;
    c->cursor_visible = 1;

    comp_damage_mark_rect(&c->damage, c->fb, &new_r);
}

void compositor_cursor_paint(compositor_t *c, const uint32_t *pixels,
                             int32_t sprite_stride_px)
{
    if (!c || !c->ready || !c->cursor_visible || !pixels)
        return;
    comp_back_blit(c->back, c->stride_px, c->cursor_rect.x, c->cursor_rect.y,
                   c->cursor_rect.w, c->cursor_rect.h, pixels, sprite_stride_px);
    comp_damage_mark_rect(&c->damage, c->fb, &c->cursor_rect);
}

void compositor_cursor_hide(compositor_t *c)
{
    if (!c || !c->ready || !c->cursor_visible)
        return;

    comp_damage_mark_rect(&c->damage, c->fb, &c->cursor_rect);
    c->cursor_visible = 0;
    c->cursor_rect.w = 0;
    c->cursor_rect.h = 0;
}

/* Relleno de restauración bajo el cursor (p. ej. color escritorio) sin full blit. */
void compositor_cursor_restore_under(compositor_t *c, uint32_t desktop_color)
{
    if (!c || !c->ready || !c->cursor_visible)
        return;
    comp_back_fill_rect(c->back, c->stride_px, &c->cursor_rect, desktop_color);
    comp_damage_mark_rect(&c->damage, c->fb, &c->cursor_rect);
}
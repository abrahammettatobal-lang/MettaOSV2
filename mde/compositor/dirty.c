/*
 * MettaOS MDE — dirty rects del compositor (SA-15)
 */
#include "dirty.h"
#include "compositor_glass.h"

void comp_damage_init(comp_damage_t *dmg)
{
    if (!dmg)
        return;
    comp_damage_reset(dmg);
}

void comp_damage_reset(comp_damage_t *dmg)
{
    if (!dmg)
        return;
    dmg->count = 0;
    dmg->full_frame = 0;
}

void comp_damage_mark_full(comp_damage_t *dmg)
{
    if (!dmg)
        return;
    dmg->full_frame = 1;
    dmg->count = 0;
}

static void comp_damage_push(comp_damage_t *dmg, const framebuffer_t *fb,
                               const fb_rect_t *rect)
{
    fb_rect_t clipped;
    uint32_t i;

    if (!dmg || !rect || dmg->full_frame)
        return;

    clipped = *rect;
    if (fb && !fb_clip_rect(&clipped, fb))
        return;
    if (!fb && (clipped.w <= 0 || clipped.h <= 0))
        return;

    comp_glass_invalidate_intersect(clipped.x, clipped.y, clipped.w, clipped.h);

    for (i = 0; i < dmg->count; i++) {
        const fb_rect_t *e = &dmg->rects[i];
        if (e->x == clipped.x && e->y == clipped.y &&
            e->w == clipped.w && e->h == clipped.h)
            return;
    }

    if (dmg->count >= COMP_MAX_DIRTY_RECTS) {
        dmg->full_frame = 1;
        dmg->count = 0;
        return;
    }

    dmg->rects[dmg->count++] = clipped;
}

void comp_damage_mark_rect(comp_damage_t *dmg, const framebuffer_t *fb,
                           const fb_rect_t *rect)
{
    comp_damage_push(dmg, fb, rect);
}

void comp_damage_mark(comp_damage_t *dmg, const framebuffer_t *fb,
                      int32_t x, int32_t y, int32_t w, int32_t h)
{
    fb_rect_t r = { x, y, w, h };
    comp_damage_push(dmg, fb, &r);
}

void comp_damage_merge(comp_damage_t *dmg)
{
    uint32_t i, j;
    int merged;

    if (!dmg || dmg->full_frame || dmg->count < 2)
        return;

    do {
        merged = 0;
        for (i = 0; i < dmg->count; i++) {
            for (j = i + 1; j < dmg->count; j++) {
                fb_rect_t u;
                fb_rect_t a = dmg->rects[i];
                fb_rect_t b = dmg->rects[j];

                fb_rect_union(&a, &b, &u);
                if (u.w * u.h <= (a.w * a.h + b.w * b.h)) {
                    dmg->rects[i] = u;
                    dmg->rects[j] = dmg->rects[dmg->count - 1];
                    dmg->count--;
                    merged = 1;
                    break;
                }
            }
            if (merged)
                break;
        }
    } while (merged && dmg->count > 1);
}

uint32_t comp_damage_count(const comp_damage_t *dmg)
{
    return dmg ? dmg->count : 0;
}

int comp_damage_is_full(const comp_damage_t *dmg)
{
    return dmg ? dmg->full_frame : 0;
}

const fb_rect_t *comp_damage_rects(const comp_damage_t *dmg, uint32_t *out_count)
{
    if (!dmg) {
        if (out_count)
            *out_count = 0;
        return NULL;
    }
    if (out_count)
        *out_count = dmg->full_frame ? 0 : dmg->count;
    return dmg->full_frame ? NULL : dmg->rects;
}
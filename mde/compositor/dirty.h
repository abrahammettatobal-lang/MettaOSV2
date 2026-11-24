/*
 * MettaOS MDE — seguimiento de daño por rectángulos (SA-15).
 * Capa independiente del dirty del framebuffer (SA-14); el compositor
 * acumula regiones a repintar en el back buffer antes del present parcial.
 */
#ifndef MDE_COMPOSITOR_DIRTY_H
#define MDE_COMPOSITOR_DIRTY_H

#include <graphics/fb.h>
#include <stdint.h>

#define COMP_MAX_DIRTY_RECTS 64u

typedef struct {
    fb_rect_t rects[COMP_MAX_DIRTY_RECTS];
    uint32_t  count;
    int       full_frame;
} comp_damage_t;

void comp_damage_init(comp_damage_t *dmg);
void comp_damage_reset(comp_damage_t *dmg);

void comp_damage_mark(comp_damage_t *dmg, const framebuffer_t *fb,
                      int32_t x, int32_t y, int32_t w, int32_t h);
void comp_damage_mark_rect(comp_damage_t *dmg, const framebuffer_t *fb,
                           const fb_rect_t *rect);
void comp_damage_mark_full(comp_damage_t *dmg);

/* Reduce solapes: unión cuando el bounding box es más barato que dos rects. */
void comp_damage_merge(comp_damage_t *dmg);

uint32_t comp_damage_count(const comp_damage_t *dmg);
int      comp_damage_is_full(const comp_damage_t *dmg);

/* Lectura segura tras merge; count puede ser 0 si full_frame. */
const fb_rect_t *comp_damage_rects(const comp_damage_t *dmg, uint32_t *out_count);

#endif /* MDE_COMPOSITOR_DIRTY_H */
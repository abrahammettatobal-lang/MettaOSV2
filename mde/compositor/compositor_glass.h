/*
 * LG-T02 — Liquid Glass: blur frosted (~gaussian), tint, specular, caché por slot.
 */
#ifndef MDE_COMPOSITOR_GLASS_H
#define MDE_COMPOSITOR_GLASS_H

#include "../gfx/draw.h"
#include "../theme/glass.h"

#include <stdint.h>

typedef enum {
    COMP_GLASS_SLOT_TOPBAR = 0,
    COMP_GLASS_SLOT_DOCK   = 1,
    COMP_GLASS_SLOT_WINDOW = 2,
    COMP_GLASS_SLOT_COUNT
} comp_glass_slot_t;

void comp_glass_init(void);
void comp_glass_invalidate_all(void);
void comp_glass_invalidate_slot(comp_glass_slot_t slot);

/*
 * Invalida slots cuya caché intersecta un rect de daño (p. ej. ventana bajo el dock).
 * Ignora rects muy pequeños (cursor) para no romper el fast-path de caché.
 */
void comp_glass_invalidate_intersect(int32_t x, int32_t y, int32_t w, int32_t h);

/*
 * Frosted blur del fondo ya pintado en la superficie + tint chrome + borde specular.
 * Si el slot tiene caché válida (geometría + escena estática), evita re-blur.
 */
void comp_glass_paint_panel(draw_surface_t *s, int32_t x, int32_t y,
                            int32_t w, int32_t h, const mde_glass_tokens *glass,
                            comp_glass_slot_t slot, uint8_t blur_radius,
                            int32_t corner_radius);

/* Menús flotantes: glass puntual sin caché regional. */
void comp_glass_paint_menu(draw_surface_t *s, int32_t x, int32_t y,
                           int32_t w, int32_t h, const mde_glass_tokens *glass);

#endif /* MDE_COMPOSITOR_GLASS_H */
#ifndef MDE_COMPOSITOR_PAINT_H
#define MDE_COMPOSITOR_PAINT_H

#include "compositor_mde.h"
#include "compositor_shell.h"

#include "../gfx/draw.h"
#include "../gfx/glyph_cache.h"

#include "compositor_glass.h"

void comp_paint_fill_alpha(draw_surface_t *s, int32_t x, int32_t y,
                           int32_t w, int32_t h, uint32_t rgb, uint8_t alpha);

struct mde_glass_tokens;

void comp_paint_dock_cmds(draw_surface_t *s, const dock_draw_list_t *list,
                          const struct mde_glass_tokens *glass);
void comp_paint_topbar_cmds(draw_surface_t *s, const topbar_draw_list_t *list,
                            const struct mde_glass_tokens *glass);

/* Listas SA-20 + texto SA-16 (glyph_cache); respeta clip de la superficie. */
void comp_paint_chrome(draw_surface_t *s, const mde_palette *pal);

/* Z1 ventanas: draw list WM chrome + titlebar Liquid Glass (SLOT_WINDOW). */
void comp_paint_windows_region(draw_surface_t *s, int32_t clip_x, int32_t clip_y,
                               int32_t clip_w, int32_t clip_h);

void comp_paint_chrome_text(draw_surface_t *s, const mde_palette *pal);

void comp_paint_ctx_menu(draw_surface_t *s, int32_t menu_x, int32_t menu_y,
                         const mde_palette *pal);

#endif /* MDE_COMPOSITOR_PAINT_H */
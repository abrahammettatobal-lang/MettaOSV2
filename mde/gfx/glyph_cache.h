/*
 * MettaOS MDE — caché de glifos rasterizados (SA-16)
 */
#ifndef MDE_GLYPH_CACHE_H
#define MDE_GLYPH_CACHE_H

#include <stdint.h>

#include "draw.h"

#define GLYPH_CACHE_CAPACITY 64u

void glyph_cache_init(void);
void glyph_cache_clear(void);

void glyph_cache_draw_glyph(draw_surface_t *dst, int32_t x, int32_t y, char ch,
                            uint32_t fg, uint32_t bg, int transparent_bg);
void glyph_cache_draw_string(draw_surface_t *dst, int32_t x, int32_t y, const char *text,
                             uint32_t fg, uint32_t bg, int transparent_bg);

uint32_t glyph_cache_hits(void);
uint32_t glyph_cache_misses(void);

#endif /* MDE_GLYPH_CACHE_H */
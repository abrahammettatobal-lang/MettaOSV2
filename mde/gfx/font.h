/*
 * MettaOS MDE — fuente bitmap 8×8 mínima, ASCII 0x20–0x7E (SA-16)
 */
#ifndef MDE_FONT_H
#define MDE_FONT_H

#include <stdint.h>

#include "draw.h"

#define MDE_FONT_CELL_W       8
#define MDE_FONT_CELL_H       8
#define MDE_FONT_ADVANCE      8
#define MDE_FONT_FIRST_CP     32
#define MDE_FONT_LAST_CP      126
#define MDE_FONT_GLYPH_COUNT  (MDE_FONT_LAST_CP - MDE_FONT_FIRST_CP + 1)

typedef struct {
    uint8_t cell_w;
    uint8_t cell_h;
    uint8_t advance;
    uint8_t first_cp;
    uint8_t last_cp;
} mde_font_metrics_t;

void font_metrics(mde_font_metrics_t *out);

/* Devuelve filas del glifo (8 bytes) o sustituto U+003F si está fuera de rango. */
const uint8_t *font_glyph_rows(char ch);

int32_t font_string_advance(const char *text);
void font_draw_glyph(draw_surface_t *dst, int32_t x, int32_t y, char ch,
                     uint32_t fg, uint32_t bg, int transparent_bg);
void font_draw_string(draw_surface_t *dst, int32_t x, int32_t y, const char *text,
                      uint32_t fg, uint32_t bg, int transparent_bg);

#endif /* MDE_FONT_H */
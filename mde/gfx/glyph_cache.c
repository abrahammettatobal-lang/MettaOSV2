/*
 * MettaOS MDE — caché LRU simple de glifos 8×8 (SA-16)
 */

#include "glyph_cache.h"

#include "font.h"

typedef struct glyph_cache_entry {
    uint8_t  ch;
    uint8_t  transparent_bg;
    uint32_t fg;
    uint32_t bg;
    uint32_t pixels[MDE_FONT_CELL_W * MDE_FONT_CELL_H];
    uint32_t age;
    uint8_t  valid;
} glyph_cache_entry_t;

static glyph_cache_entry_t g_cache[GLYPH_CACHE_CAPACITY];
static uint32_t g_cache_clock;
static uint32_t g_cache_hits;
static uint32_t g_cache_misses;

static void glyph_cache_rasterize(glyph_cache_entry_t *entry, char ch, uint32_t fg,
                                  uint32_t bg, int transparent_bg)
{
    const uint8_t *rows;
    int32_t row;
    int32_t col;

    rows = font_glyph_rows(ch);
    entry->ch = (uint8_t)ch;
    entry->fg = fg;
    entry->bg = bg;
    entry->transparent_bg = (uint8_t)(transparent_bg ? 1 : 0);

    for (row = 0; row < MDE_FONT_CELL_H; row++) {
        uint8_t bits = rows[row];
        for (col = 0; col < MDE_FONT_CELL_W; col++) {
            uint32_t pix;
            uint32_t mask = 0x80u >> (uint32_t)col;

            if (bits & mask)
                pix = fg;
            else if (transparent_bg)
                pix = 0u;
            else
                pix = bg;

            entry->pixels[(uint32_t)row * MDE_FONT_CELL_W + (uint32_t)col] = pix;
        }
    }
    entry->valid = 1;
    entry->age = ++g_cache_clock;
}

static glyph_cache_entry_t *glyph_cache_lookup(char ch, uint32_t fg, uint32_t bg,
                                                 int transparent_bg)
{
    uint32_t i;
    glyph_cache_entry_t *oldest = &g_cache[0];

    for (i = 0; i < GLYPH_CACHE_CAPACITY; i++) {
        glyph_cache_entry_t *e = &g_cache[i];

        if (e->valid && e->ch == (uint8_t)ch && e->fg == fg && e->bg == bg &&
            e->transparent_bg == (uint8_t)(transparent_bg ? 1 : 0)) {
            e->age = ++g_cache_clock;
            g_cache_hits++;
            return e;
        }

        if (!e->valid || e->age < oldest->age)
            oldest = e;
    }

    g_cache_misses++;
    glyph_cache_rasterize(oldest, ch, fg, bg, transparent_bg);
    return oldest;
}

void glyph_cache_init(void)
{
    glyph_cache_clear();
}

void glyph_cache_clear(void)
{
    uint32_t i;

    g_cache_clock = 0;
    g_cache_hits = 0;
    g_cache_misses = 0;
    for (i = 0; i < GLYPH_CACHE_CAPACITY; i++)
        g_cache[i].valid = 0;
}

static void glyph_cache_blit_entry(draw_surface_t *dst, int32_t x, int32_t y,
                                   const glyph_cache_entry_t *entry, int transparent_bg)
{
    int32_t row;
    int32_t col;

    for (row = 0; row < MDE_FONT_CELL_H; row++) {
        for (col = 0; col < MDE_FONT_CELL_W; col++) {
            uint32_t c = entry->pixels[(uint32_t)row * MDE_FONT_CELL_W + (uint32_t)col];
            if (transparent_bg && c == 0u)
                continue;
            draw_pixel(dst, x + col, y + row, c);
        }
    }
}

void glyph_cache_draw_glyph(draw_surface_t *dst, int32_t x, int32_t y, char ch,
                            uint32_t fg, uint32_t bg, int transparent_bg)
{
    const glyph_cache_entry_t *entry;

    if (!dst)
        return;

    fg = draw_color_from_theme(fg);
    bg = draw_color_from_theme(bg);

    entry = glyph_cache_lookup(ch, fg, bg, transparent_bg);
    glyph_cache_blit_entry(dst, x, y, entry, transparent_bg);
}

void glyph_cache_draw_string(draw_surface_t *dst, int32_t x, int32_t y, const char *text,
                             uint32_t fg, uint32_t bg, int transparent_bg)
{
    int32_t pen_x = x;

    if (!dst || !text)
        return;

    fg = draw_color_from_theme(fg);
    bg = draw_color_from_theme(bg);

    while (*text) {
        glyph_cache_draw_glyph(dst, pen_x, y, *text, fg, bg, transparent_bg);
        pen_x += MDE_FONT_ADVANCE;
        text++;
    }
}

uint32_t glyph_cache_hits(void)
{
    return g_cache_hits;
}

uint32_t glyph_cache_misses(void)
{
    return g_cache_misses;
}
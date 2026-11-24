/*
 * MettaOS MDE — primitivas 2D en buffer CPU (SA-16)
 * Consumidor: compositor SA-15, shell y apps nativas.
 */
#ifndef MDE_DRAW_H
#define MDE_DRAW_H

#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} draw_rect_t;

typedef struct draw_surface {
    uint32_t *pixels;
    int32_t   width;
    int32_t   height;
    int32_t   stride; /* píxeles por fila */
    draw_rect_t clip;
    int       clip_enabled;
} draw_surface_t;

/* Color canónico MDE en superficie: 0x00RRGGBB (alineado con fb_rgb, SA-14). */
static inline uint32_t draw_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Normaliza ARGB opaco de mde/theme (0xFFRRGGBB) a 0x00RRGGBB. */
static inline uint32_t draw_color_from_theme(uint32_t argb)
{
    return argb & 0x00FFFFFFu;
}

/* ARGB32 tema Liquid Glass: 0xAARRGGBB. */
static inline uint8_t draw_argb_alpha(uint32_t argb)
{
    return (uint8_t)(argb >> 24);
}

static inline uint32_t draw_argb_rgb(uint32_t argb)
{
    return argb & 0x00FFFFFFu;
}

/* Mezcla src sobre dst (RGB 0x00RRGGBB); alpha 0–255. */
uint32_t draw_blend_rgb(uint32_t dst_rgb, uint32_t src_rgb, uint8_t alpha);

void draw_surface_init(draw_surface_t *s, uint32_t *pixels,
                       int32_t width, int32_t height, int32_t stride);

void draw_reset_clip(draw_surface_t *s);
void draw_set_clip_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h);

int draw_clip_rect(draw_rect_t *r, const draw_surface_t *s);
void draw_rect_union(const draw_rect_t *a, const draw_rect_t *b, draw_rect_t *out);

void draw_pixel(draw_surface_t *s, int32_t x, int32_t y, uint32_t color);
void draw_clear(draw_surface_t *s, uint32_t color);

void draw_fill_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                    uint32_t color);
void draw_blend_pixel(draw_surface_t *s, int32_t x, int32_t y, uint32_t argb);
void draw_fill_rect_alpha(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                          uint32_t argb);
void draw_stroke_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                      uint32_t color, int32_t thickness);

void draw_hline(draw_surface_t *s, int32_t x, int32_t y, int32_t w, uint32_t color);
void draw_vline(draw_surface_t *s, int32_t x, int32_t y, int32_t h, uint32_t color);
void draw_line(draw_surface_t *s, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
               uint32_t color);

void draw_fill_rounded_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                            int32_t radius, uint32_t color);
void draw_fill_rounded_rect_alpha(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                                  int32_t radius, uint32_t argb);
void draw_stroke_rounded_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                              int32_t radius, uint32_t color, int32_t thickness);

/* Liquid Glass — frosted blur CPU, sombra, panel compuesto */
void draw_frosted_rect(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                       int32_t radius, int32_t kernel_size, uint32_t tint_argb);
void draw_specular_edge(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                        int32_t radius, uint32_t specular_argb, int32_t height_px);
void draw_drop_shadow(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                      int32_t radius);
void draw_glass_panel(draw_surface_t *s, int32_t x, int32_t y, int32_t w, int32_t h,
                      int32_t radius, uint32_t fill_argb, uint32_t border_argb);

void draw_blit(draw_surface_t *dst, int32_t dst_x, int32_t dst_y,
               const uint32_t *src, int32_t src_w, int32_t src_h, int32_t src_stride);

#endif /* MDE_DRAW_H */
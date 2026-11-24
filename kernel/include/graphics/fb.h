/*
 * MettaOS — framebuffer lineal (Multiboot2 tag 8). SA-14
 * Contrato con compositor MDE: ver docs/swarm/sa-14-report.md
 */
#ifndef METTA_FB_H
#define METTA_FB_H

#include <stddef.h>
#include <stdint.h>

#define FB_MULTIBOOT_TAG_FRAMEBUFFER 8u

#define FB_MAX_DIRTY_RECTS 32u

/* Multiboot2 framebuffer_type */
#define FB_MB_TYPE_INDEXED 0u
#define FB_MB_TYPE_RGB     1u
#define FB_MB_TYPE_EGA     2u

typedef enum {
    FB_FORMAT_UNKNOWN = 0,
    FB_FORMAT_XRGB8888,
    FB_FORMAT_RGB888,
    FB_FORMAT_RGB565,
} fb_pixel_format_t;

typedef struct {
    uint32_t *base;
    uint64_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch; /* bytes per scanline */
    uint8_t  bpp;
    uint8_t  mb_type;
    fb_pixel_format_t format;
    int      ready;
} framebuffer_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} fb_rect_t;

typedef struct {
    fb_rect_t rects[FB_MAX_DIRTY_RECTS];
    uint32_t  count;
    int       full_frame;
} fb_dirty_t;

/* Mapea dirección física del FB a puntero escribible (identidad por defecto). */
typedef void *(*fb_phys_map_fn)(uint64_t phys, uint64_t size);

int fb_init_from_multiboot(const void *multiboot_info);
void fb_set_phys_mapper(fb_phys_map_fn mapper);

const framebuffer_t *fb_get(void);
fb_dirty_t *fb_dirty_get(void);

void fb_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void fb_blit(int32_t dst_x, int32_t dst_y, int32_t src_w, int32_t src_h,
             const uint32_t *src_pixels, int32_t src_stride_px);

void fb_mark_dirty(const fb_rect_t *rect);
void fb_mark_dirty_rect(int32_t x, int32_t y, int32_t w, int32_t h);
void fb_mark_full_framebuffer_dirty(void);

/* Cierra el frame: fusiona dirty rects y las deja listas para SA-15. */
void fb_present(void);

/* Utilidades de recorte (también usadas por el compositor). */
int fb_clip_rect(fb_rect_t *r, const framebuffer_t *fb);
void fb_rect_union(const fb_rect_t *a, const fb_rect_t *b, fb_rect_t *out);

/* Color MettaOS: 0x00RRGGBB */
static inline uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

#endif /* METTA_FB_H */
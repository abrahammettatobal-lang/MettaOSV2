/* MettaOS MDE — wallpaper Tahoe / Liquid Glass (LG-T06) */

#ifndef METTAOS_MDE_SHELL_WALLPAPER_H
#define METTAOS_MDE_SHELL_WALLPAPER_H

#include <stdint.h>

#define WALLPAPER_FORMAT_RGBA8888 0u /* uint32: 0xRRGGBBAA */

typedef struct wallpaper_image {
    const uint32_t *pixels;
    uint32_t        width;
    uint32_t        height;
    uint32_t        format;
} wallpaper_image_t;

/** Carga `assets/wallpaper/light_default.h` como fondo activo. */
int wallpaper_load_default(void);

/** Imagen activa o NULL si no hay carga. */
const wallpaper_image_t *wallpaper_get_active(void);

void wallpaper_unload(void);

/**
 * Muestreo bilineal RGBA (punto fijo). Sin imagen → perla escritorio #F6F8FB.
 */
uint32_t wallpaper_sample_bilinear(const wallpaper_image_t *img,
                                   int32_t dst_x,
                                   int32_t dst_y,
                                   int32_t dst_w,
                                   int32_t dst_h);

/** Blit escalado a buffer 0x00RRGGBB. */
void wallpaper_blit_bilinear(const wallpaper_image_t *img,
                           uint32_t *dst,
                           int32_t dst_w,
                           int32_t dst_h,
                           int32_t dst_stride_px);

/** Blit escalado conservando RGBA8888. */
void wallpaper_blit_bilinear_rgba(const wallpaper_image_t *img,
                                  uint32_t *dst,
                                  int32_t dst_w,
                                  int32_t dst_h,
                                  int32_t dst_stride_px);

#endif /* METTAOS_MDE_SHELL_WALLPAPER_H */
/* MettaOS MDE — wallpaper Tahoe Liquid Glass (LG-T06 / LG-07) */

#include "wallpaper.h"

#include <stddef.h>
#include <stdint.h>

#include "../../assets/wallpaper/light_default.h"

#define WALLPAPER_FP_SHIFT 16
#define WALLPAPER_FP_ONE   (1u << WALLPAPER_FP_SHIFT)

static wallpaper_image_t g_wallpaper_active;

static uint8_t wp_chan_lerp_u8(uint8_t a, uint8_t b, uint32_t t_fp)
{
    uint32_t inv = WALLPAPER_FP_ONE - t_fp;

    return (uint8_t)(((uint32_t)a * inv + (uint32_t)b * t_fp) >> WALLPAPER_FP_SHIFT);
}

static uint32_t wp_sample_bilinear_core(const wallpaper_image_t *img,
                                        uint32_t src_x_fp,
                                        uint32_t src_y_fp)
{
    uint32_t x0;
    uint32_t y0;
    uint32_t x1;
    uint32_t y1;
    uint32_t tx_fp;
    uint32_t ty_fp;
    uint32_t c00;
    uint32_t c10;
    uint32_t c01;
    uint32_t c11;
    uint8_t  r_top;
    uint8_t  g_top;
    uint8_t  b_top;
    uint8_t  a_top;
    uint8_t  r_bot;
    uint8_t  g_bot;
    uint8_t  b_bot;
    uint8_t  a_bot;

    if (img == NULL || img->pixels == NULL || img->width == 0u || img->height == 0u) {
        return 0xF6F8FBFFu;
    }

    if (img->width == 1u && img->height == 1u) {
        return img->pixels[0];
    }

    x0 = src_x_fp >> WALLPAPER_FP_SHIFT;
    y0 = src_y_fp >> WALLPAPER_FP_SHIFT;
    tx_fp = src_x_fp & (WALLPAPER_FP_ONE - 1u);
    ty_fp = src_y_fp & (WALLPAPER_FP_ONE - 1u);

    if (x0 >= img->width - 1u) {
        x0 = img->width - 2u;
        tx_fp = WALLPAPER_FP_ONE - 1u;
    }
    if (y0 >= img->height - 1u) {
        y0 = img->height - 2u;
        ty_fp = WALLPAPER_FP_ONE - 1u;
    }

    x1 = x0 + 1u;
    y1 = y0 + 1u;

    c00 = img->pixels[y0 * img->width + x0];
    c10 = img->pixels[y0 * img->width + x1];
    c01 = img->pixels[y1 * img->width + x0];
    c11 = img->pixels[y1 * img->width + x1];

    r_top = wp_chan_lerp_u8((uint8_t)((c00 >> 24) & 0xFFu),
                            (uint8_t)((c10 >> 24) & 0xFFu), tx_fp);
    g_top = wp_chan_lerp_u8((uint8_t)((c00 >> 16) & 0xFFu),
                            (uint8_t)((c10 >> 16) & 0xFFu), tx_fp);
    b_top = wp_chan_lerp_u8((uint8_t)((c00 >> 8) & 0xFFu),
                            (uint8_t)((c10 >> 8) & 0xFFu), tx_fp);
    a_top = wp_chan_lerp_u8((uint8_t)(c00 & 0xFFu), (uint8_t)(c10 & 0xFFu), tx_fp);

    r_bot = wp_chan_lerp_u8((uint8_t)((c01 >> 24) & 0xFFu),
                            (uint8_t)((c11 >> 24) & 0xFFu), tx_fp);
    g_bot = wp_chan_lerp_u8((uint8_t)((c01 >> 16) & 0xFFu),
                            (uint8_t)((c11 >> 16) & 0xFFu), tx_fp);
    b_bot = wp_chan_lerp_u8((uint8_t)((c01 >> 8) & 0xFFu),
                            (uint8_t)((c11 >> 8) & 0xFFu), tx_fp);
    a_bot = wp_chan_lerp_u8((uint8_t)(c01 & 0xFFu), (uint8_t)(c11 & 0xFFu), tx_fp);

    return ((uint32_t)wp_chan_lerp_u8(r_top, r_bot, ty_fp) << 24) |
           ((uint32_t)wp_chan_lerp_u8(g_top, g_bot, ty_fp) << 16) |
           ((uint32_t)wp_chan_lerp_u8(b_top, b_bot, ty_fp) << 8) |
           (uint32_t)wp_chan_lerp_u8(a_top, a_bot, ty_fp);
}

int wallpaper_load_default(void)
{
    g_wallpaper_active.pixels = wallpaper_light_default_rgba;
    g_wallpaper_active.width  = WALLPAPER_LIGHT_DEFAULT_WIDTH;
    g_wallpaper_active.height = WALLPAPER_LIGHT_DEFAULT_HEIGHT;
    g_wallpaper_active.format = WALLPAPER_LIGHT_DEFAULT_FORMAT;
    return 0;
}

const wallpaper_image_t *wallpaper_get_active(void)
{
    if (g_wallpaper_active.pixels == NULL) {
        return NULL;
    }
    return &g_wallpaper_active;
}

void wallpaper_unload(void)
{
    g_wallpaper_active.pixels = NULL;
    g_wallpaper_active.width  = 0;
    g_wallpaper_active.height = 0;
    g_wallpaper_active.format = 0;
}

uint32_t wallpaper_sample_bilinear(const wallpaper_image_t *img,
                                   int32_t dst_x,
                                   int32_t dst_y,
                                   int32_t dst_w,
                                   int32_t dst_h)
{
    const wallpaper_image_t *src = img;
    uint32_t                 src_x_fp;
    uint32_t                 src_y_fp;
    int32_t                  denom_w;
    int32_t                  denom_h;

    if (src == NULL) {
        src = wallpaper_get_active();
    }
    if (src == NULL || dst_w <= 0 || dst_h <= 0) {
        return 0xF6F8FBFFu;
    }

    denom_w = dst_w - 1;
    denom_h = dst_h - 1;
    if (denom_w < 1) {
        denom_w = 1;
    }
    if (denom_h < 1) {
        denom_h = 1;
    }

    if (src->width <= 1u || src->height <= 1u) {
        if (src->pixels != NULL) {
            return src->pixels[0];
        }
        return 0xF6F8FBFFu;
    }

    src_x_fp = (uint32_t)(((int64_t)dst_x * (int64_t)(src->width - 1u) *
                           (int64_t)WALLPAPER_FP_ONE) /
                          (int64_t)denom_w);
    src_y_fp = (uint32_t)(((int64_t)dst_y * (int64_t)(src->height - 1u) *
                           (int64_t)WALLPAPER_FP_ONE) /
                          (int64_t)denom_h);

    return wp_sample_bilinear_core(src, src_x_fp, src_y_fp);
}

void wallpaper_blit_bilinear(const wallpaper_image_t *img,
                             uint32_t *dst,
                             int32_t dst_w,
                             int32_t dst_h,
                             int32_t dst_stride_px)
{
    int32_t x;
    int32_t y;

    if (dst == NULL || dst_w <= 0 || dst_h <= 0) {
        return;
    }
    if (dst_stride_px <= 0) {
        dst_stride_px = dst_w;
    }

    for (y = 0; y < dst_h; y++) {
        uint32_t *row = dst + (int64_t)y * (int64_t)dst_stride_px;
        for (x = 0; x < dst_w; x++) {
            uint32_t rgba = wallpaper_sample_bilinear(img, x, y, dst_w, dst_h);
            uint8_t  r    = (uint8_t)((rgba >> 24) & 0xFFu);
            uint8_t  g    = (uint8_t)((rgba >> 16) & 0xFFu);
            uint8_t  b    = (uint8_t)((rgba >> 8) & 0xFFu);
            row[x]        = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
}

void wallpaper_blit_bilinear_rgba(const wallpaper_image_t *img,
                                  uint32_t *dst,
                                  int32_t dst_w,
                                  int32_t dst_h,
                                  int32_t dst_stride_px)
{
    int32_t x;
    int32_t y;

    if (dst == NULL || dst_w <= 0 || dst_h <= 0) {
        return;
    }
    if (dst_stride_px <= 0) {
        dst_stride_px = dst_w;
    }

    for (y = 0; y < dst_h; y++) {
        uint32_t *row = dst + (int64_t)y * (int64_t)dst_stride_px;
        for (x = 0; x < dst_w; x++) {
            row[x] = wallpaper_sample_bilinear(img, x, y, dst_w, dst_h);
        }
    }
}
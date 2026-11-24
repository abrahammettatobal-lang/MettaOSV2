/* MettaOS kernel — bootsplash claro Tahoe / MDE Light (LG-T06) */

#include "bootsplash.h"
#include <brand_tiles.h>
#include "graphics/fb.h"
#include "log/klog.h"

#include <stddef.h>
#include <stdint.h>

#define BOOTSPLASH_PROGRESS_MAX 1000u

#define BOOTSPLASH_BG         0x00F6F8FBu
#define BOOTSPLASH_TRACK      0x00DCE2EAu
#define BOOTSPLASH_ACCENT     0x003278D9u
#define BOOTSPLASH_TEXT       0x002D3640u
#define BOOTSPLASH_SUBTEXT    0x006B7785u
#define BOOTSPLASH_SLOT_BORDER 0x00DCE2EAu

static bootsplash_phase_t g_bootsplash_phase = BOOTSPLASH_PHASE_EARLY;
static uint64_t           g_bootsplash_mde_ready_ms;
static uint32_t           g_bootsplash_progress;
static int                g_bootsplash_released;
static int                g_background_ready;
static uint32_t           g_painted_progress;

/* 5×7 bitmap minúsculas (bit0 = fila superior, bit4 = izquierda). */
static const uint8_t g_glyph_5x7[26][7] = {
    { 0x1Fu, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x1Fu }, /* a */
    { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x11u, 0x11u, 0x1Eu }, /* b */
    { 0x0Eu, 0x11u, 0x10u, 0x10u, 0x10u, 0x11u, 0x0Eu }, /* c */
    { 0x1Eu, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x1Eu }, /* d */
    { 0x1Fu, 0x10u, 0x10u, 0x1Eu, 0x10u, 0x10u, 0x1Fu }, /* e */
    { 0x1Eu, 0x10u, 0x10u, 0x1Eu, 0x10u, 0x10u, 0x10u }, /* f */
    { 0x0Eu, 0x11u, 0x10u, 0x17u, 0x11u, 0x11u, 0x0Fu }, /* g */
    { 0x11u, 0x11u, 0x11u, 0x1Fu, 0x11u, 0x11u, 0x11u }, /* h */
    { 0x0Eu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x0Eu }, /* i */
    { 0x07u, 0x02u, 0x02u, 0x02u, 0x02u, 0x12u, 0x0Cu }, /* j */
    { 0x11u, 0x12u, 0x14u, 0x18u, 0x14u, 0x12u, 0x11u }, /* k */
    { 0x10u, 0x10u, 0x10u, 0x10u, 0x10u, 0x10u, 0x1Fu }, /* l */
    { 0x11u, 0x1Bu, 0x15u, 0x15u, 0x11u, 0x11u, 0x11u }, /* m */
    { 0x11u, 0x19u, 0x15u, 0x13u, 0x11u, 0x11u, 0x11u }, /* n */
    { 0x0Eu, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Eu }, /* o */
    { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x10u, 0x10u, 0x10u }, /* p */
    { 0x0Eu, 0x11u, 0x11u, 0x11u, 0x15u, 0x12u, 0x0Du }, /* q */
    { 0x1Eu, 0x11u, 0x11u, 0x1Eu, 0x14u, 0x12u, 0x11u }, /* r */
    { 0x0Fu, 0x10u, 0x10u, 0x0Eu, 0x01u, 0x01u, 0x1Eu }, /* s */
    { 0x1Fu, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u }, /* t */
    { 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Eu }, /* u */
    { 0x11u, 0x11u, 0x11u, 0x11u, 0x11u, 0x0Au, 0x04u }, /* v */
    { 0x11u, 0x11u, 0x11u, 0x15u, 0x15u, 0x15u, 0x0Au }, /* w */
    { 0x11u, 0x11u, 0x0Au, 0x04u, 0x0Au, 0x11u, 0x11u }, /* x */
    { 0x11u, 0x11u, 0x11u, 0x0Au, 0x04u, 0x04u, 0x04u }, /* y */
    { 0x1Fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x1Fu }, /* z */
};

static int bootsplash_glyph_index(char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return (int)(ch - 'a');
    }
    if (ch >= 'A' && ch <= 'Z') {
        return (int)(ch - 'A');
    }
    return -1;
}

static void bootsplash_put_pixel(int32_t x, int32_t y, uint32_t color)
{
    const framebuffer_t *fb = fb_get();

    if (fb == NULL || !fb->ready || fb->base == NULL) {
        return;
    }
    if (x < 0 || y < 0 || x >= (int32_t)fb->width || y >= (int32_t)fb->height) {
        return;
    }

    {
        uint32_t *row =
            (uint32_t *)((uint8_t *)fb->base + (uint64_t)y * (uint64_t)fb->pitch);
        row[x] = color;
    }
}

static void bootsplash_fill_hgradient_rect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t row;
    int32_t col;
    const framebuffer_t *fb = fb_get();

    if (fb == NULL || !fb->ready || w <= 0 || h <= 0) {
        return;
    }

    /* Gradiente Tahoe: perla #FAFCFF → cielo #E2EEF9 (sin float / libgcc). */
    for (row = 0; row < h; row++) {
        uint32_t t_row = (h > 1) ? (uint32_t)((row * 255) / (h - 1)) : 0u;
        uint8_t  rb    = (uint8_t)(0xFAu - (t_row * 24u) / 255u);
        uint8_t  gb    = (uint8_t)(0xFCu - (t_row * 18u) / 255u);
        uint8_t  bb    = (uint8_t)(0xFFu - (t_row * 10u) / 255u);
        uint32_t glow  = (t_row > 140u) ? ((t_row - 140u) * 35u) / 115u : 0u;

        for (col = 0; col < w; col++) {
            uint32_t t_col = (w > 1) ? (uint32_t)((col * 200) / (w - 1)) : 0u;
            int32_t  cx    = (int32_t)col - (int32_t)w / 2;
            int32_t  cy    = (int32_t)row - (int32_t)(h * 28u / 100u);
            uint32_t blob  = 0u;

            if (cx >= -90 && cx <= 90 && cy >= -40 && cy <= 40) {
                uint32_t nx = (uint32_t)(cx * cx);
                uint32_t ny = (uint32_t)(cy * cy);
                if (nx / 90u + ny / 40u < 90u) {
                    blob = (90u - (nx / 90u + ny / 40u)) / 3u;
                }
            }

            {
                uint8_t r = (uint8_t)(rb - (t_col * 12u) / 200u);
                uint8_t g = (uint8_t)(gb - (t_col * 8u) / 200u);
                uint32_t blue = bb + glow + blob;
                uint8_t b = blue > 255u ? 255u : (uint8_t)blue;
                bootsplash_put_pixel(x + col, y + row, fb_rgb(r, g, b));
            }
        }
    }
}

static void bootsplash_draw_glyph(int32_t x, int32_t y, int32_t scale, char ch, uint32_t color)
{
    int gi;
    int row;
    int col;

    gi = bootsplash_glyph_index(ch);
    if (gi < 0 || scale <= 0) {
        return;
    }

    for (row = 0; row < 7; row++) {
        uint8_t bits = g_glyph_5x7[gi][row];
        for (col = 0; col < 5; col++) {
            if ((bits & (1u << (4 - col))) == 0u) {
                continue;
            }
            {
                int32_t px;
                int32_t py;
                int32_t sx;
                int32_t sy;

                for (sy = 0; sy < scale; sy++) {
                    for (sx = 0; sx < scale; sx++) {
                        px = x + col * scale + sx;
                        py = y + row * scale + sy;
                        bootsplash_put_pixel(px, py, color);
                    }
                }
            }
        }
    }
}

static void bootsplash_draw_text(int32_t x, int32_t y, int32_t scale, const char *text,
                                 uint32_t color)
{
    int32_t cx = x;

    if (text == NULL || scale <= 0) {
        return;
    }

    while (*text != '\0') {
        bootsplash_draw_glyph(cx, y, scale, *text, color);
        cx += 6 * scale;
        ++text;
    }
}

static void bootsplash_draw_logo_slot(int32_t cx, int32_t cy)
{
    for (int y=0; y<BRAND_SIDE; y++)
        for (int x=0; x<BRAND_SIDE; x++) {
            int i=y*BRAND_SIDE+x;
            if (brand_alpha[i]>=128)
                bootsplash_put_pixel(cx+x-BRAND_SIDE/2,cy+y-BRAND_SIDE/2,brand_color(0,i));
        }
}

static void bootsplash_draw_progress(int32_t cx, int32_t y, int32_t track_w, int32_t track_h)
{
    int32_t x0 = cx - track_w / 2;
    int32_t fill_w;
    uint32_t permille;

    if (track_w <= 0 || track_h <= 0) {
        return;
    }

    fb_fill_rect(x0, y, track_w, track_h, BOOTSPLASH_TRACK);

    permille = g_bootsplash_progress;
    if (permille > BOOTSPLASH_PROGRESS_MAX) {
        permille = BOOTSPLASH_PROGRESS_MAX;
    }

    fill_w = (int32_t)((track_w * (int32_t)permille) / (int32_t)BOOTSPLASH_PROGRESS_MAX);
    if (fill_w > 2) {
        fb_fill_rect(x0 + 1, y + 1, fill_w - 2, track_h - 2, BOOTSPLASH_ACCENT);
    }
}

void bootsplash_set_progress(uint32_t permille)
{
    if (permille > BOOTSPLASH_PROGRESS_MAX) {
        permille = BOOTSPLASH_PROGRESS_MAX;
    }
    g_bootsplash_progress = permille;
}

void bootsplash_paint(void)
{
    const framebuffer_t *fb = fb_get();
    int32_t              cx;
    int32_t              cy;

    if (fb == NULL || !fb->ready) {
        return;
    }

    if (g_bootsplash_released && g_bootsplash_phase == BOOTSPLASH_PHASE_RELEASED) {
        return;
    }

    if (g_background_ready) {
        if (g_painted_progress != g_bootsplash_progress) {
            int32_t width = (int32_t)fb->width / 3;
            cx = (int32_t)fb->width / 2;
            cy = (int32_t)fb->height / 2 + 72;
            bootsplash_draw_progress(cx, cy, width, 6);
            fb_mark_dirty_rect(cx - width / 2, cy, width, 6);
            g_painted_progress = g_bootsplash_progress;
        }
        return;
    }
    g_background_ready = 1;
    g_painted_progress = g_bootsplash_progress;
    bootsplash_fill_hgradient_rect(0, 0, (int32_t)fb->width, (int32_t)fb->height);

    cx = (int32_t)fb->width / 2;
    cy = (int32_t)fb->height / 2;

    bootsplash_draw_logo_slot(cx, cy - 36);
    bootsplash_draw_text(cx - 42, cy + 42, 2, "mettaos", BOOTSPLASH_TEXT);
    bootsplash_draw_progress(cx, cy + 72, (int32_t)fb->width / 3, 6);
    fb_mark_full_framebuffer_dirty();
}

void bootsplash_on_mde_ready(uint64_t monotonic_ms)
{
    g_bootsplash_mde_ready_ms = monotonic_ms;
    g_bootsplash_phase        = BOOTSPLASH_PHASE_HOLD;
    KLOG_BOOT("bootsplash: mde ready, hold %u ms\n", (unsigned)BOOTSPLASH_POST_MDE_HOLD_MS);
    bootsplash_set_progress(1000u);
    bootsplash_paint();
}

bootsplash_phase_t bootsplash_phase(void)
{
    return g_bootsplash_phase;
}

int bootsplash_should_hold(uint64_t monotonic_ms)
{
    uint64_t elapsed;

    if (g_bootsplash_released || g_bootsplash_phase == BOOTSPLASH_PHASE_RELEASED) {
        return 0;
    }

    if (g_bootsplash_phase != BOOTSPLASH_PHASE_HOLD) {
        return (g_bootsplash_phase == BOOTSPLASH_PHASE_EARLY ||
                g_bootsplash_phase == BOOTSPLASH_PHASE_MDE);
    }

    if (monotonic_ms < g_bootsplash_mde_ready_ms) {
        return 1;
    }

    elapsed = monotonic_ms - g_bootsplash_mde_ready_ms;
    if (elapsed >= (uint64_t)BOOTSPLASH_POST_MDE_HOLD_MS) {
        return 0;
    }
    return 1;
}

void bootsplash_release(void)
{
    g_bootsplash_released = 1;
    g_bootsplash_phase    = BOOTSPLASH_PHASE_RELEASED;
    KLOG_BOOT("bootsplash: released\n");
}

void bootsplash_tick(uint64_t monotonic_ms)
{
    (void)monotonic_ms;

    if (g_bootsplash_released) {
        return;
    }

    if (g_bootsplash_phase == BOOTSPLASH_PHASE_HOLD && !bootsplash_should_hold(monotonic_ms)) {
        bootsplash_release();
        return;
    }

    if (g_bootsplash_phase != BOOTSPLASH_PHASE_RELEASED) {
        bootsplash_paint();
    }
}

void bootsplash_early_init(void)
{
    g_bootsplash_phase    = BOOTSPLASH_PHASE_EARLY;
    g_bootsplash_progress = 0u;
    g_background_ready = 0;
    g_bootsplash_released = 0;
    KLOG_BOOT("bootsplash: early paint\n");
    bootsplash_paint();
}
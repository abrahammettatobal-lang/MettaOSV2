/*
 * MettaOS shell — tipos internos compartidos entre módulos ui_*.c
 */
#ifndef MDE_UI_INTERNAL_H
#define MDE_UI_INTERNAL_H

#include "ui.h"

#include <stddef.h>
#include <stdint.h>

void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);

/* ---------- canvas ---------- */

typedef struct {
    uint32_t *px;
    int32_t   w, h, stride;
    int32_t   cx0, cy0, cx1, cy1; /* clip [cx0,cx1) × [cy0,cy1) */
} ui_canvas_t;

typedef struct {
    int32_t x, y, w, h;
} ui_rect_t;

static inline int ui_rect_has(const ui_rect_t *r, int32_t px, int32_t py)
{
    return px >= r->x && py >= r->y && px < r->x + r->w && py < r->y + r->h;
}

static inline int ui_rects_overlap(const ui_rect_t *a, const ui_rect_t *b)
{
    return a->w > 0 && a->h > 0 && b->w > 0 && b->h > 0 &&
           a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

void ui_rect_union(ui_rect_t *acc, const ui_rect_t *r);

static inline int32_t ui_min(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t ui_max(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t ui_clampi(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float ui_clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float ui_absf(float v) { return v < 0.0f ? -v : v; }

float ui_sqrtf(float v);
float ui_sinf(float v);
float ui_cosf(float v);

#define UI_RGB(r, g, b) ((((uint32_t)(r)) << 16) | (((uint32_t)(g)) << 8) | (uint32_t)(b))

uint32_t ui_mix(uint32_t a, uint32_t b, uint32_t t255);

void ui_canvas_clip(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, int32_t h);
void ui_fill(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb, uint32_t a);
void ui_rrect(ui_canvas_t *c, float x, float y, float w, float h, float r, uint32_t rgb, uint32_t a);
void ui_rrect_grad(ui_canvas_t *c, float x, float y, float w, float h, float r,
                   uint32_t top, uint32_t bottom, uint32_t a);
void ui_rrect_stroke(ui_canvas_t *c, float x, float y, float w, float h, float r,
                     float thick, uint32_t rgb, uint32_t a);
void ui_circle(ui_canvas_t *c, float cx, float cy, float rad, uint32_t rgb, uint32_t a);
void ui_ring(ui_canvas_t *c, float cx, float cy, float rad, float thick, uint32_t rgb, uint32_t a);
void ui_capsule(ui_canvas_t *c, float x0, float y0, float x1, float y1, float thick,
                uint32_t rgb, uint32_t a);
void ui_arc(ui_canvas_t *c, float cx, float cy, float rad, float thick,
            float half_angle_cos, uint32_t rgb, uint32_t a); /* arco superior */
void ui_shadow(ui_canvas_t *c, float x, float y, float w, float h, float r,
               float blur, float offy, uint32_t a);

typedef struct {
    int32_t  blur;       /* radio de caja (3 pasadas) */
    uint32_t tint;       /* color de tinte */
    uint32_t tint_a;     /* 0..255 */
    float    saturation; /* 1 = sin cambio */
    float    lens;       /* px de refracción en el borde */
    float    rim;        /* intensidad del brillo especular 0..1 */
    int      dark;       /* borde/rim para vidrio oscuro */
} ui_glass_t;

void ui_glass(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, int32_t h, float r,
              const ui_glass_t *g);

/* ---------- texto ---------- */

enum {
    UI_FONT_UI = 0,
    UI_FONT_UI_BOLD,
    UI_FONT_SMALL,
    UI_FONT_TITLE,
    UI_FONT_LARGE,
    UI_FONT_MONO,
    UI_FONT_HUGE,
};

int32_t ui_text(ui_canvas_t *c, int32_t x, int32_t y, const char *s, int font,
                uint32_t rgb, uint32_t a);
int32_t ui_text_n(ui_canvas_t *c, int32_t x, int32_t y, const char *s, int32_t nbytes,
                  int font, uint32_t rgb, uint32_t a);
int32_t ui_text_width(const char *s, int font);
int32_t ui_text_width_n(const char *s, int32_t nbytes, int font);
int32_t ui_font_line(int font);
int32_t ui_font_ascent(int font);
void    ui_text_center(ui_canvas_t *c, int32_t cx, int32_t y, const char *s, int font,
                       uint32_t rgb, uint32_t a);
/* Recorta con "…" para caber en max_w. */
void    ui_text_fit(ui_canvas_t *c, int32_t x, int32_t y, const char *s, int font,
                    uint32_t rgb, uint32_t a, int32_t max_w);

uint32_t ui_utf8_next(const char *s, int32_t *i);
int32_t  ui_utf8_prev(const char *s, int32_t i);
int32_t  ui_utf8_encode(uint32_t cp, char *out);

/* ---------- wallpaper ---------- */

#define UI_WALL_COUNT 4
const char *ui_wall_name(int idx);
void ui_wall_render(int idx, uint32_t *dst, int32_t w, int32_t h, int32_t stride,
                    int32_t full_w, int32_t full_h);

/* ---------- utilidades ---------- */

void ui_strlcpy(char *dst, const char *src, size_t cap);
void ui_strlcat(char *dst, const char *src, size_t cap);
int  ui_streq(const char *a, const char *b);
int  ui_str_starts(const char *s, const char *prefix);
int  ui_str_icontains(const char *hay, const char *needle);
void ui_itoa(int64_t v, char *out);
void ui_fmt2(char *out, int v); /* 2 dígitos */

typedef struct {
    int year, month, day, hour, minute, second, wday;
} ui_time_t;
void ui_rtc_read(ui_time_t *t);

/* ---------- iconos ---------- */

enum {
    UI_ICON_FILES = 0,
    UI_ICON_NOTES,
    UI_ICON_TERMINAL,
    UI_ICON_CALC,
    UI_ICON_SETTINGS,
    UI_ICON_TRASH,
    UI_ICON_FOLDER,
    UI_ICON_DOC,
    UI_ICON_APPIMAGE,
    UI_ICON_ABOUT,
    UI_ICON_NETWORK,
};
void ui_icon_draw(ui_canvas_t *c, int icon, float x, float y, float size);
void ui_logo_draw(ui_canvas_t *c, float cx, float cy, float size, uint32_t rgb, uint32_t a);

#endif /* MDE_UI_INTERNAL_H */

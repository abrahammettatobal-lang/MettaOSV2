/*
 * MettaOS shell — utilidades compartidas por las apps.
 */
#ifndef MDE_UI_APPS_COMMON_H
#define MDE_UI_APPS_COMMON_H

#include "ui_app.h"

void app_pill(ui_canvas_t *c, int32_t x, int32_t y, int32_t w, int32_t h, int hover, int dark);
void app_chevron(ui_canvas_t *c, float cx, float cy, int left, uint32_t col, uint32_t a);
void app_plus(ui_canvas_t *c, float cx, float cy, float s, uint32_t col, uint32_t a);
void app_trash_glyph(ui_canvas_t *c, float cx, float cy, uint32_t col, uint32_t a);
void app_compose_glyph(ui_canvas_t *c, float cx, float cy, uint32_t col, uint32_t a);

void ui_fmt_num(double v, char *out);
int  ui_eval(const char *s, double *out);
void ui_memmove(char *dst, const char *src, int32_t n);

static inline int app_hit(int32_t x, int32_t y, int32_t rx, int32_t ry, int32_t rw, int32_t rh)
{
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

#endif /* MDE_UI_APPS_COMMON_H */

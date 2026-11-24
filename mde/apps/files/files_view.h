#ifndef MDE_FILES_VIEW_H
#define MDE_FILES_VIEW_H

#include "files_model.h"
#include "files_theme.h"

#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} mde_files_rect;

typedef struct {
    void *ctx;
    void (*fill_rect)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void (*stroke_rect)(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void (*draw_text)(void *ctx, int32_t x, int32_t y, const char *text, uint32_t color);
} mde_files_painter;

typedef struct {
    mde_files_chrome chrome;

    mde_files_rect client;
    mde_files_rect sidebar;
    mde_files_rect toolbar;
    mde_files_rect path_field;
    mde_files_rect list_header;
    mde_files_rect list_body;

    int32_t visible_rows;
} mde_files_view_layout;

void mde_files_view_layout_compute(mde_files_view_layout *lay, int32_t cw, int32_t ch);

void mde_files_view_paint(
    const mde_files_view_layout *lay,
    const mde_files_model *model,
    const mde_files_painter *painter);

int32_t mde_files_view_hit_row(const mde_files_view_layout *lay, const mde_files_model *model, int32_t y);

#endif /* MDE_FILES_VIEW_H */
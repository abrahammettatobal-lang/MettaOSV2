/*
 * Pintado MDE Light en back buffer (SA-15 + SA-20 chrome + SA-16 texto).
 */
#include "compositor_paint.h"

#include "../gfx/font.h"
#include "../theme/controls.h"
#include "../theme/glass.h"
#include "../theme/theme.h"

#include "compositor_glass.h"

#include <stddef.h>

/* kind en draw lists SA-20: 0 = panel/barra glass, 1 = borde (specular glass) */
#define COMP_SHELL_CMD_GLASS_PANEL 0u
#define COMP_SHELL_CMD_BORDER      1u

/* Bitmap de ícono: MDE_DOCK_ICON_DRAW_PX (40) en dock.h; slot hit = MDE_DOCK_ICON_PX (44). */

/* Enlazado fuerte opcional desde mde/shell/dock.c */
__attribute__((weak)) uint32_t dock_item_count(void)
{
    const dock_layout_t *l = dock_layout_compute();

    if (l == NULL || l->item_count <= 0)
        return 0u;
    return (uint32_t)l->item_count;
}

__attribute__((weak)) const char *dock_item_label(uint32_t index)
{
    (void)index;
    return NULL;
}

static uint32_t comp_alpha_blend(uint32_t dst, uint32_t src_rgb, uint8_t alpha)
{
    uint32_t da, dr, dg, db;
    uint32_t sr, sg, sb;
    uint32_t a;
    uint32_t inv;

    if (alpha >= 255u)
        return src_rgb;
    if (alpha == 0u)
        return dst;

    da = 0xffu;
    dr = (dst >> 16) & 0xffu;
    dg = (dst >> 8) & 0xffu;
    db = dst & 0xffu;

    sr = (src_rgb >> 16) & 0xffu;
    sg = (src_rgb >> 8) & 0xffu;
    sb = src_rgb & 0xffu;

    a = alpha;
    inv = 255u - a;

    dr = (sr * a + dr * inv) / 255u;
    dg = (sg * a + dg * inv) / 255u;
    db = (sb * a + db * inv) / 255u;

    return (da << 24) | (dr << 16) | (dg << 8) | db;
}

static int comp_rect_intersects_clip(const draw_surface_t *s,
                                     int32_t x, int32_t y, int32_t w, int32_t h)
{
    draw_rect_t r;
    if (!s || w <= 0 || h <= 0)
        return 0;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    if (s->clip_enabled && !draw_clip_rect(&r, s))
        return 0;
    return 1;
}

void comp_paint_fill_alpha(draw_surface_t *s, int32_t x, int32_t y,
                           int32_t w, int32_t h, uint32_t rgb, uint8_t alpha)
{
    int32_t row, col;

    if (!s || !s->pixels || w <= 0 || h <= 0)
        return;
    if (!comp_rect_intersects_clip(s, x, y, w, h))
        return;

    rgb = draw_color_from_theme(rgb);
    if (alpha >= 255u) {
        draw_fill_rect(s, x, y, w, h, rgb);
        return;
    }

    for (row = 0; row < h; row++) {
        int32_t sy = y + row;
        uint32_t *row_px = s->pixels + (uint64_t)sy * (uint64_t)s->stride;
        for (col = 0; col < w; col++) {
            int32_t sx = x + col;
            row_px[sx] = comp_alpha_blend(row_px[sx], rgb, alpha);
        }
    }
}

static int comp_cmd_is_dock_glass_panel(const dock_rect_cmd_t *cmd,
                                         const dock_layout_t *layout)
{
    if (!cmd || !layout)
        return 0;
    if (cmd->kind != COMP_SHELL_CMD_GLASS_PANEL)
        return 0;
    return (cmd->x == layout->x && cmd->y == layout->y &&
            cmd->w == layout->w && cmd->h == layout->h);
}

static int comp_cmd_is_topbar_glass_panel(const topbar_rect_cmd_t *cmd,
                                           const topbar_layout_t *layout)
{
    if (!cmd || !layout)
        return 0;
    if (cmd->kind != COMP_SHELL_CMD_GLASS_PANEL)
        return 0;
    return (cmd->x == layout->x && cmd->y == layout->y &&
            cmd->w == layout->w && cmd->h == layout->h);
}

void comp_paint_dock_cmds(draw_surface_t *s, const dock_draw_list_t *list,
                          const mde_glass_tokens *glass)
{
    const dock_layout_t *layout;
    const dock_style_t *style;
    uint32_t i;
    int glass_done;

    if (!s || !list)
        return;

    layout = dock_layout_compute();
    style = dock_get_style();
    glass_done = 0;

    for (i = 0; i < list->count; i++) {
        const dock_rect_cmd_t *cmd = &list->cmds[i];

        if (cmd->kind == COMP_SHELL_CMD_BORDER)
            continue;

        if (glass != NULL && !glass_done &&
            comp_cmd_is_dock_glass_panel(cmd, layout)) {
            uint8_t blur = glass->blur_heavy;
            int32_t radius = style ? (int32_t)style->radius : 10;

            comp_glass_paint_panel(s, cmd->x, cmd->y, cmd->w, cmd->h, glass,
                                   COMP_GLASS_SLOT_DOCK, blur, radius);
            if (radius > 0)
                draw_stroke_rounded_rect(s, cmd->x, cmd->y, cmd->w, cmd->h, radius,
                                         draw_color_from_theme(glass->border_base),
                                         (int32_t)glass->border_width);
            glass_done = 1;
            continue;
        }

        if (comp_cmd_is_dock_glass_panel(cmd, layout))
            continue;

        {
            uint32_t color = draw_color_from_theme(cmd->color);
            if (cmd->alpha < 255u)
                comp_paint_fill_alpha(s, cmd->x, cmd->y, cmd->w, cmd->h,
                                      color, cmd->alpha);
            else
                draw_fill_rect(s, cmd->x, cmd->y, cmd->w, cmd->h, color);
        }
    }
}

void comp_paint_topbar_cmds(draw_surface_t *s, const topbar_draw_list_t *list,
                            const mde_glass_tokens *glass)
{
    const topbar_layout_t *layout;
    uint32_t i;
    int glass_done;

    if (!s || !list)
        return;

    layout = topbar_layout_compute();
    glass_done = 0;

    for (i = 0; i < list->count; i++) {
        const topbar_rect_cmd_t *cmd = &list->cmds[i];

        if (cmd->kind == COMP_SHELL_CMD_BORDER)
            continue;

        if (glass != NULL && !glass_done &&
            comp_cmd_is_topbar_glass_panel(cmd, layout)) {
            comp_glass_paint_panel(s, cmd->x, cmd->y, cmd->w, cmd->h, glass,
                                   COMP_GLASS_SLOT_TOPBAR, glass->blur_light, 0);
            glass_done = 1;
            continue;
        }

        if (comp_cmd_is_topbar_glass_panel(cmd, layout))
            continue;

        {
            uint32_t color = draw_color_from_theme(cmd->color);
            draw_fill_rect(s, cmd->x, cmd->y, cmd->w, cmd->h, color);
        }
    }
}

void comp_paint_chrome_text(draw_surface_t *s, const mde_palette *pal)
{
    const topbar_layout_t *tl;
    int32_t text_y;
    uint32_t fg;
    uint32_t muted;
    int32_t pen_x;
    uint32_t mi;

    if (!s || !pal)
        return;

    tl = topbar_layout_compute();
    if (!tl || tl->h <= 0)
        return;

    text_y = tl->y + (tl->h - MDE_FONT_CELL_H) / 2;
    fg = draw_color_from_theme(pal->text);
    muted = draw_color_from_theme(pal->muted);

    glyph_cache_draw_string(s, tl->logo_x + 24, text_y, "Metta", fg, 0, 1);

    if (topbar_active_app() != NULL && topbar_active_app()[0] != '\0')
        glyph_cache_draw_string(s, tl->app_x, text_y, topbar_active_app(), fg, 0, 1);

    pen_x = tl->menus_x;
    for (mi = 0; mi < topbar_menu_count(); mi++) {
        const char *lab = topbar_menu_label(mi);

        if (lab == NULL)
            continue;
        glyph_cache_draw_string(s, pen_x, text_y, lab, muted, 0, 1);
        pen_x += font_string_advance(lab) + 10;
    }

    if (topbar_clock_text() != NULL)
        glyph_cache_draw_string(s, tl->clock_x, text_y, topbar_clock_text(), muted, 0, 1);

    glyph_cache_draw_string(s, tl->config_x + 4, text_y, "Cfg", muted, 0, 1);
}

static void comp_paint_dock_icon_labels(draw_surface_t *s,
                                        const dock_draw_list_t *list,
                                        const mde_palette *pal)
{
    uint32_t i;
    uint32_t icon_idx;
    uint32_t fg;

    if (!s || !list || !pal)
        return;

    fg = draw_color_from_theme(pal->text);
    icon_idx = 0;

    for (i = 0; i < list->count; i++) {
        const dock_rect_cmd_t *cmd = &list->cmds[i];
        const char *lab;

        if (cmd->w != (int32_t)MDE_DOCK_ICON_DRAW_PX ||
            cmd->h != (int32_t)MDE_DOCK_ICON_DRAW_PX)
            continue;
        if (icon_idx >= dock_item_count())
            break;

        lab = dock_item_label(icon_idx);
        icon_idx++;
        if (lab == NULL || lab[0] == '\0')
            continue;

        glyph_cache_draw_string(s, cmd->x + 4,
                                cmd->y + (int32_t)MDE_DOCK_ICON_DRAW_PX - 10,
                                lab, fg, 0, 1);
    }
}

void comp_paint_chrome(draw_surface_t *s, const mde_palette *pal)
{
    dock_draw_list_t dock;
    topbar_draw_list_t top;
    const mde_glass_tokens *glass;

    if (!s || !pal)
        return;

    glass = theme_glass(theme_get_default());

    if (topbar_build_draw_list(&top) == 0)
        comp_paint_topbar_cmds(s, &top, glass);

    if (dock_build_draw_list(&dock) == 0) {
        comp_paint_dock_cmds(s, &dock, glass);
        comp_paint_dock_icon_labels(s, &dock, pal);
    }

    comp_paint_chrome_text(s, pal);
}

void comp_paint_ctx_menu(draw_surface_t *s, int32_t menu_x, int32_t menu_y,
                         const mde_palette *pal)
{
    int32_t mx, my, mw, mh;
    uint32_t i;
    int32_t row_y;
    uint32_t item_h;

    if (!s || !pal || !mde_ctxmenu_is_open())
        return;

    mde_ctxmenu_bounds(&mx, &my, &mw, &mh);
    (void)menu_x;
    (void)menu_y;

    {
        const mde_glass_tokens *glass = theme_glass(theme_get_default());

        if (glass != NULL)
            comp_glass_paint_menu(s, mx, my, mw, mh, glass);
        else {
            draw_fill_rounded_rect(s, mx, my, mw, mh, 6,
                                   draw_color_from_theme(pal->surface));
            draw_stroke_rounded_rect(s, mx, my, mw, mh, 6,
                                     draw_color_from_theme(pal->border), 1);
        }
    }

    row_y = my + 1;
    item_h = mde_ctxmenu_item_height();

    for (i = 0; i < mde_ctxmenu_item_count(); i++) {
        if (mde_ctxmenu_item_is_separator(i)) {
            row_y += 9;
            continue;
        }

        if ((int32_t)i == mde_ctxmenu_hovered_index()) {
            draw_fill_rect(s, mx + 1, row_y, mw - 2, (int32_t)item_h,
                           draw_color_from_theme(pal->hover));
        }

        {
            const char *label = mde_ctxmenu_item_label(i);

            if (label != NULL && label[0] != '\0') {
                uint32_t tfg = draw_color_from_theme(pal->text);

                glyph_cache_draw_string(s, mx + 12, row_y + 10, label, tfg, 0, 1);
            }
        }

        row_y += (int32_t)item_h;
    }
}
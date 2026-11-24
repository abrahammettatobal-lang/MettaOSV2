/*
 * Z1 — ventanas WM chrome + titlebar Liquid Glass (COMP_GLASS_SLOT_WINDOW).
 * Archivo aparte: window.h redefine mde_rect incompatible con mde.h del shell.
 */
#include "../gfx/draw.h"
#include "../gfx/glyph_cache.h"
#include "../theme/glass.h"
#include "../theme/theme.h"
#include "../wm/chrome.h"
#include "../wm/focus.h"
#include "../wm/window.h"

#include "compositor_glass.h"

#include <stddef.h>

static uint32_t comp_argb(uint32_t rgb, uint8_t alpha)
{
    return ((uint32_t)alpha << 24) | (rgb & 0x00FFFFFFu);
}

static void comp_paint_wm_chrome_list(draw_surface_t *s,
                                      const mde_window_t *win,
                                      const mde_wm_chrome_draw_list_t *list,
                                      const mde_glass_tokens *glass)
{
    uint32_t i;
    int titlebar_glass_done = 0;

    if (!s || !win || !list)
        return;

    for (i = 0; i < list->count; i++) {
        const mde_wm_chrome_rect_cmd_t *cmd = &list->cmds[i];
        const mde_rect_t *r = &cmd->rect;
        uint32_t color = draw_color_from_theme(cmd->color);
        int32_t radius = (int32_t)cmd->corner_radius;

        if (r->w <= 0 || r->h <= 0)
            continue;

        switch (cmd->kind) {
        case MDE_WM_CHROME_CMD_SHADOW:
            if (cmd->alpha > 0u)
                draw_fill_rounded_rect_alpha(s, r->x, r->y, r->w, r->h, radius,
                                             comp_argb(color, cmd->alpha));
            break;

        case MDE_WM_CHROME_CMD_FILL:
            if (cmd->blur_radius > 0u && glass != NULL && !titlebar_glass_done) {
                comp_glass_invalidate_slot(COMP_GLASS_SLOT_WINDOW);
                comp_glass_paint_panel(s, r->x, r->y, r->w, r->h, glass,
                                       COMP_GLASS_SLOT_WINDOW, cmd->blur_radius,
                                       radius);
                titlebar_glass_done = 1;
            } else if (cmd->alpha >= 255u) {
                if (radius > 0)
                    draw_fill_rounded_rect(s, r->x, r->y, r->w, r->h, radius, color);
                else
                    draw_fill_rect(s, r->x, r->y, r->w, r->h, color);
            } else {
                draw_fill_rounded_rect_alpha(s, r->x, r->y, r->w, r->h, radius,
                                             comp_argb(color, cmd->alpha));
            }
            break;

        case MDE_WM_CHROME_CMD_SPECULAR:
            if (!titlebar_glass_done && cmd->alpha > 0u)
                draw_fill_rect_alpha(s, r->x, r->y, r->w, r->h,
                                     comp_argb(color, cmd->alpha));
            break;

        case MDE_WM_CHROME_CMD_BORDER:
            if (radius > 0)
                draw_stroke_rounded_rect(s, r->x, r->y, r->w, r->h, radius, color, 1);
            else
                draw_stroke_rect(s, r->x, r->y, r->w, r->h, color, 1);
            break;

        case MDE_WM_CHROME_CMD_CTRL:
            if (radius > 0)
                draw_fill_rounded_rect(s, r->x, r->y, r->w, r->h, radius, color);
            else
                draw_fill_rect(s, r->x, r->y, r->w, r->h, color);
            break;

        case MDE_WM_CHROME_CMD_TITLE: {
            int32_t ty = r->y + (r->h > 8 ? (r->h - 8) / 2 : 0);

            if (win->title[0] != '\0')
                glyph_cache_draw_string(s, r->x, ty, win->title, color, 0, 1);
            break;
        }

        case MDE_WM_CHROME_CMD_GRIP:
            if (cmd->alpha > 0u)
                draw_fill_rect_alpha(s, r->x, r->y, r->w, r->h,
                                     comp_argb(color, cmd->alpha));
            else
                draw_fill_rect(s, r->x, r->y, r->w, r->h, color);
            break;

        default:
            break;
        }
    }
}

void comp_paint_windows_region(draw_surface_t *s, int32_t clip_x, int32_t clip_y,
                               int32_t clip_w, int32_t clip_h)
{
    uint32_t ids[MDE_WM_MAX_WINDOWS];
    uint32_t n = 0;
    uint32_t i;
    uint32_t focused;
    const mde_glass_tokens *glass;

    (void)clip_x;
    (void)clip_y;
    (void)clip_w;
    (void)clip_h;

    if (!s)
        return;

    if (wm_window_stack_bottom_to_top(ids, MDE_WM_MAX_WINDOWS, &n) != 0 || n == 0u)
        return;

    focused = wm_focus_get();
    glass = theme_glass(theme_get_default());

    for (i = 0; i < n; i++) {
        mde_window_t *win = wm_window_get(ids[i]);
        mde_wm_chrome_draw_list_t list;

        if (win == NULL || win->state == MDE_WIN_STATE_MINIMIZED)
            continue;

        if (wm_chrome_build_draw_list(win, focused, &list) < 0)
            continue;

        comp_paint_wm_chrome_list(s, win, &list, glass);
    }
}

/* MettaOS MDE — dock flotante inferior Liquid Glass (LG-T03 Tahoe) */

#include "dock.h"

#include "../theme/controls.h"
#include "../theme/glass.h"
#include "../theme/theme.h"

#include <stddef.h>
#include <stdint.h>

#define MDE_DOCK_MAX_ITEMS         16u
#define MDE_DOCK_MIN_WIDTH         120u
#define MDE_DOCK_HOVER_SCALE_PCT   108u /* hover sutil ~1.08× sobre el slot */
#define MDE_DOCK_ACTIVE_PILL_W     10
#define MDE_DOCK_ACTIVE_PILL_H     4
#define MDE_DOCK_RUNNING_DOT       4

typedef struct dock_item {
    const char *app_id;
    const char *label;
    int         running;
    int         active;
} dock_item_t;

static const struct {
    const char *app_id;
    const char *label;
} k_dock_default_items[] = {
    { "mde.notes",    "Notes" },
    { "mde.files",    "Files" },
    { "mde.terminal", "Terminal" },
};

static dock_item_t   g_dock_items[MDE_DOCK_MAX_ITEMS];
static uint32_t      g_dock_item_count;
static dock_style_t  g_dock_style;
static int32_t       g_viewport_w;
static int32_t       g_viewport_h;
static dock_layout_t g_dock_layout;
static int32_t       g_hovered_index;
static int           g_labels_visible;

static uint8_t dock_argb_alpha(uint32_t argb)
{
    return (uint8_t)((argb >> 24) & 0xffu);
}

static uint32_t dock_argb_rgb(uint32_t argb)
{
    return argb & 0x00ffffffu;
}

static void dock_style_apply_glass(void)
{
    mde_glass_dock_style glass;
    const mde_glass_tokens *tok = &mde_light_glass;

    mde_glass_dock_style_light(&glass);

    g_dock_style.material         = DOCK_MATERIAL_GLASS;
    g_dock_style.fill             = dock_argb_rgb(glass.surface.fill);
    g_dock_style.fill_alpha       = dock_argb_alpha(glass.surface.fill);
    g_dock_style.border           = dock_argb_rgb(glass.surface.border_base);
    g_dock_style.border_specular  = dock_argb_rgb(glass.surface.border_specular);
    g_dock_style.specular_top     = dock_argb_rgb(glass.surface.specular_top);
    g_dock_style.shadow           = MDE_COLOR_RGB(0x000000u);
    g_dock_style.accent_glass     = dock_argb_rgb(tok->accent);
    g_dock_style.accent_alpha     = dock_argb_alpha(tok->accent);
    g_dock_style.radius           = glass.surface.corner_radius;
    g_dock_style.padding          = glass.padding;
    g_dock_style.margin_bottom    = glass.margin_bottom;
    g_dock_style.border_width     = glass.surface.border_width;
    g_dock_style.blur_radius      = glass.surface.blur_radius;
}

static int32_t dock_icon_slot_px(void)
{
    return (int32_t)MDE_DOCK_ICON_PX;
}

static int32_t dock_icon_draw_px(void)
{
    return (int32_t)MDE_DOCK_ICON_DRAW_PX;
}

static int32_t dock_item_stride_px(void)
{
    return dock_icon_slot_px() + (int32_t)MDE_DOCK_ITEM_GAP_PX;
}

static int32_t dock_panel_content_h(void)
{
    int32_t h = dock_icon_slot_px() + (int32_t)MDE_DOCK_INDICATOR_STRIP_PX;

    if (g_labels_visible) {
        h += (int32_t)MDE_DOCK_LABEL_BAND_PX;
    }
    return h;
}

static uint32_t dock_content_width(uint32_t count)
{
    uint32_t icons;
    uint32_t gaps;
    uint32_t pad;
    uint32_t w;

    if (count == 0u) {
        return MDE_DOCK_MIN_WIDTH;
    }

    icons = count * (uint32_t)MDE_DOCK_ICON_PX;
    gaps = (count > 1u) ? (count - 1u) * MDE_DOCK_ITEM_GAP_PX : 0u;
    pad  = (uint32_t)g_dock_style.padding * 2u;
    w    = icons + gaps + pad;
    if (w < MDE_DOCK_MIN_WIDTH) {
        return MDE_DOCK_MIN_WIDTH;
    }
    return w;
}

static int dock_str_eq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

static int dock_clip_push(dock_draw_list_t *list, dock_rect_cmd_t cmd)
{
    if (list == NULL || list->count >= MDE_DOCK_MAX_DRAW_CMDS) {
        return -1;
    }
    list->cmds[list->count++] = cmd;
    return 0;
}

static int dock_slot_origin(uint32_t index, int32_t *slot_x, int32_t *slot_y)
{
    const dock_layout_t *layout;
    int32_t              pad;
    int32_t              stride;

    if (index >= g_dock_item_count || slot_x == NULL || slot_y == NULL) {
        return -1;
    }

    layout = dock_layout_compute();
    if (layout == NULL) {
        return -1;
    }

    pad    = (int32_t)g_dock_style.padding;
    stride = dock_item_stride_px();
    *slot_x = layout->x + pad + (int32_t)index * stride;
    *slot_y = layout->y + layout->icon_y;
    return 0;
}

static void dock_seed_default_items(void)
{
    uint32_t i;

    dock_clear_items();
    for (i = 0u; i < (sizeof(k_dock_default_items) / sizeof(k_dock_default_items[0])); ++i) {
        if (dock_item_add(k_dock_default_items[i].app_id,
                          k_dock_default_items[i].label,
                          0) != 0) {
            break;
        }
    }
}

void dock_init(void)
{
    g_viewport_w      = 0;
    g_viewport_h      = 0;
    g_hovered_index   = -1;
    g_labels_visible  = 0;
    dock_style_apply_glass();
    g_dock_layout.x          = 0;
    g_dock_layout.y          = 0;
    g_dock_layout.w          = 0;
    g_dock_layout.h          = 0;
    g_dock_layout.item_count = 0;
    g_dock_layout.icon_size  = dock_icon_slot_px();
    g_dock_layout.item_stride = dock_item_stride_px();
    g_dock_layout.icon_y     = 0;
    dock_seed_default_items();
}

void dock_set_viewport(int32_t width, int32_t height)
{
    if (width < 0) {
        width = 0;
    }
    if (height < 0) {
        height = 0;
    }
    g_viewport_w = width;
    g_viewport_h = height;
}

void dock_set_hovered_index(int32_t index)
{
    if (index < 0 || (uint32_t)index >= g_dock_item_count) {
        g_hovered_index = -1;
        return;
    }
    g_hovered_index = index;
}

int32_t dock_hovered_index(void)
{
    return g_hovered_index;
}

void dock_set_labels_visible(int visible)
{
    g_labels_visible = visible ? 1 : 0;
}

int dock_labels_visible(void)
{
    return g_labels_visible;
}

uint8_t dock_get_material(void)
{
    return g_dock_style.material;
}

int dock_item_add(const char *app_id, const char *label, int running)
{
    if (app_id == NULL || label == NULL || g_dock_item_count >= MDE_DOCK_MAX_ITEMS) {
        return -1;
    }
    dock_item_t *it = &g_dock_items[g_dock_item_count++];
    it->app_id   = app_id;
    it->label    = label;
    it->running  = running ? 1 : 0;
    it->active   = 0;
    return 0;
}

void dock_clear_items(void)
{
    g_dock_item_count = 0u;
    g_hovered_index   = -1;
}

void dock_set_active_app(const char *app_id)
{
    uint32_t i;

    for (i = 0u; i < g_dock_item_count; ++i) {
        g_dock_items[i].active =
            (app_id != NULL && dock_str_eq(app_id, g_dock_items[i].app_id)) ? 1 : 0;
    }
}

int dock_item_set_running(const char *app_id, int running)
{
    uint32_t i;

    if (app_id == NULL) {
        return -1;
    }
    for (i = 0u; i < g_dock_item_count; ++i) {
        if (dock_str_eq(app_id, g_dock_items[i].app_id)) {
            g_dock_items[i].running = running ? 1 : 0;
            return 0;
        }
    }
    return -1;
}

uint32_t dock_item_count(void)
{
    return g_dock_item_count;
}

const char *dock_item_label(uint32_t index)
{
    if (index >= g_dock_item_count) {
        return NULL;
    }
    return g_dock_items[index].label;
}

const char *dock_item_app_id(uint32_t index)
{
    if (index >= g_dock_item_count) {
        return NULL;
    }
    return g_dock_items[index].app_id;
}

int32_t dock_index_for_app_id(const char *app_id)
{
    uint32_t i;

    if (app_id == NULL) {
        return -1;
    }
    for (i = 0u; i < g_dock_item_count; ++i) {
        if (dock_str_eq(app_id, g_dock_items[i].app_id)) {
            return (int32_t)i;
        }
    }
    return -1;
}

int dock_item_is_active(uint32_t index)
{
    if (index >= g_dock_item_count) {
        return 0;
    }
    return g_dock_items[index].active;
}

int dock_item_is_running(uint32_t index)
{
    if (index >= g_dock_item_count) {
        return 0;
    }
    return g_dock_items[index].running;
}

int dock_item_hit_rect(uint32_t index, int32_t *out_x, int32_t *out_y,
                       int32_t *out_w, int32_t *out_h)
{
    int32_t sx;
    int32_t sy;

    if (out_x == NULL || out_y == NULL || out_w == NULL || out_h == NULL) {
        return -1;
    }
    if (dock_slot_origin(index, &sx, &sy) != 0) {
        return -1;
    }
    *out_x = sx;
    *out_y = sy;
    *out_w = dock_icon_slot_px();
    *out_h = dock_icon_slot_px() + (int32_t)MDE_DOCK_INDICATOR_STRIP_PX;
    return 0;
}

int dock_item_icon_draw_rect(uint32_t index, int32_t *out_x, int32_t *out_y,
                             int32_t *out_w, int32_t *out_h)
{
    int32_t sx;
    int32_t sy;
    int32_t inset;

    if (out_x == NULL || out_y == NULL || out_w == NULL || out_h == NULL) {
        return -1;
    }
    if (dock_slot_origin(index, &sx, &sy) != 0) {
        return -1;
    }
    inset = (dock_icon_slot_px() - dock_icon_draw_px()) / 2;
    if (inset < 0) {
        inset = 0;
    }
    *out_x = sx + inset;
    *out_y = sy + inset;
    *out_w = dock_icon_draw_px();
    *out_h = dock_icon_draw_px();
    return 0;
}

int dock_item_label_anchor(uint32_t index, int32_t *out_x, int32_t *out_y)
{
    int32_t sx;
    int32_t sy;

    if (out_x == NULL || out_y == NULL) {
        return -1;
    }
    if (!g_labels_visible) {
        return -1;
    }
    if (dock_slot_origin(index, &sx, &sy) != 0) {
        return -1;
    }
    *out_x = sx + 2;
    *out_y = sy + dock_icon_slot_px() + (int32_t)MDE_DOCK_INDICATOR_STRIP_PX;
    return 0;
}

int32_t dock_hit_test(int32_t x, int32_t y)
{
    const dock_layout_t *layout = dock_layout_compute();
    uint32_t             i;
    int32_t              hx;
    int32_t              hy;
    int32_t              hw;
    int32_t              hh;

    if (layout == NULL || g_dock_item_count == 0u) {
        return -1;
    }
    if (x < layout->x || y < layout->y ||
        x >= layout->x + layout->w || y >= layout->y + layout->h) {
        return -1;
    }

    for (i = 0u; i < g_dock_item_count; ++i) {
        if (dock_item_hit_rect(i, &hx, &hy, &hw, &hh) != 0) {
            continue;
        }
        if (g_hovered_index >= 0 && (int32_t)i == g_hovered_index) {
            const int32_t pad =
                ((dock_icon_slot_px() * (int32_t)MDE_DOCK_HOVER_SCALE_PCT) / 100 -
                 dock_icon_slot_px()) /
                2;
            hx -= pad;
            hy -= pad;
            hw += pad * 2;
            hh += pad * 2;
        }
        if (x >= hx && x < hx + hw && y >= hy && y < hy + hh) {
            return (int32_t)i;
        }
    }
    return -1;
}

const dock_style_t *dock_get_style(void)
{
    return &g_dock_style;
}

const dock_layout_t *dock_layout_compute(void)
{
    const uint32_t count    = g_dock_item_count;
    const uint32_t cw       = dock_content_width(count);
    const int32_t  content_h = dock_panel_content_h();
    const int32_t  panel_h =
        content_h + (int32_t)g_dock_style.padding * 2;

    g_dock_layout.w           = (int32_t)cw;
    g_dock_layout.h           = panel_h;
    g_dock_layout.item_count  = (int32_t)count;
    g_dock_layout.icon_size   = dock_icon_slot_px();
    g_dock_layout.item_stride = dock_item_stride_px();
    g_dock_layout.icon_y      = (int32_t)g_dock_style.padding;

    if (g_viewport_w > 0 && g_viewport_h > 0) {
        g_dock_layout.x = (g_viewport_w - g_dock_layout.w) / 2;
        g_dock_layout.y =
            g_viewport_h - panel_h - (int32_t)g_dock_style.margin_bottom;
    } else {
        g_dock_layout.x = 0;
        g_dock_layout.y = 0;
    }

    return &g_dock_layout;
}

int dock_build_draw_list(dock_draw_list_t *out)
{
    const dock_layout_t *layout;
    const dock_style_t  *style;
    const mde_glass_tokens *tok;
    uint32_t i;

    if (out == NULL) {
        return -1;
    }
    out->count = 0u;

    layout = dock_layout_compute();
    style  = &g_dock_style;
    tok    = &mde_light_glass;

    for (i = 0u; i < MDE_GLASS_SHADOW_LAYERS; ++i) {
        const mde_glass_shadow_layer *sh = &tok->shadow[i];
        dock_rect_cmd_t shadow = {
            .x     = layout->x + (int32_t)sh->offset_x,
            .y     = layout->y + (int32_t)sh->offset_y,
            .w     = layout->w,
            .h     = layout->h,
            .color = style->shadow,
            .alpha = sh->alpha,
            .kind  = 0u,
        };
        if (dock_clip_push(out, shadow) != 0) {
            return -2;
        }
    }

    {
        dock_rect_cmd_t panel = {
            .x     = layout->x,
            .y     = layout->y,
            .w     = layout->w,
            .h     = layout->h,
            .color = style->fill,
            .alpha = style->fill_alpha,
            .kind  = 0u,
        };
        if (dock_clip_push(out, panel) != 0) {
            return -2;
        }
    }

    {
        const int32_t spec_h = (int32_t)tok->specular_top_height;
        dock_rect_cmd_t spec = {
            .x     = layout->x,
            .y     = layout->y,
            .w     = layout->w,
            .h     = spec_h > 0 ? spec_h : (int32_t)style->border_width,
            .color = style->specular_top,
            .alpha = dock_argb_alpha(tok->specular_top),
            .kind  = 3u,
        };
        if (dock_clip_push(out, spec) != 0) {
            return -2;
        }
    }

    {
        const int32_t bw = (int32_t)style->border_width;
        dock_rect_cmd_t border_top = {
            .x     = layout->x,
            .y     = layout->y,
            .w     = layout->w,
            .h     = bw,
            .color = style->border_specular,
            .alpha = dock_argb_alpha(tok->border_specular),
            .kind  = 1u,
        };
        dock_rect_cmd_t border_bottom = {
            .x     = layout->x,
            .y     = layout->y + layout->h - bw,
            .w     = layout->w,
            .h     = bw,
            .color = style->border,
            .alpha = dock_argb_alpha(tok->border_base),
            .kind  = 1u,
        };
        dock_rect_cmd_t border_left = {
            .x     = layout->x,
            .y     = layout->y,
            .w     = bw,
            .h     = layout->h,
            .color = style->border_specular,
            .alpha = dock_argb_alpha(tok->border_specular),
            .kind  = 1u,
        };
        dock_rect_cmd_t border_right = {
            .x     = layout->x + layout->w - bw,
            .y     = layout->y,
            .w     = bw,
            .h     = layout->h,
            .color = style->border,
            .alpha = dock_argb_alpha(tok->border_base),
            .kind  = 1u,
        };
        if (dock_clip_push(out, border_top) != 0 ||
            dock_clip_push(out, border_bottom) != 0 ||
            dock_clip_push(out, border_left) != 0 ||
            dock_clip_push(out, border_right) != 0) {
            return -2;
        }
    }

    {
        for (i = 0u; i < g_dock_item_count; ++i) {
            const dock_item_t *it = &g_dock_items[i];
            int32_t            slot_x;
            int32_t            slot_y;
            int32_t            icon_x;
            int32_t            icon_y;
            int32_t            draw;
            const mde_palette *p = theme_palette(theme_get_default());

            if (dock_slot_origin(i, &slot_x, &slot_y) != 0) {
                continue;
            }

            draw = dock_icon_draw_px();

            if (g_hovered_index >= 0 && (int32_t)i == g_hovered_index) {
                const int32_t scaled =
                    (dock_icon_slot_px() * (int32_t)MDE_DOCK_HOVER_SCALE_PCT) / 100;
                const int32_t pad = (scaled - dock_icon_slot_px()) / 2;

                dock_rect_cmd_t hover_glass = {
                    .x     = slot_x - pad,
                    .y     = slot_y - pad,
                    .w     = scaled,
                    .h     = scaled,
                    .color = dock_argb_rgb(MDE_GLASS_FILL_CHROME),
                    .alpha = dock_argb_alpha(MDE_GLASS_FILL_CHROME),
                    .kind  = 4u,
                };
                if (dock_clip_push(out, hover_glass) != 0) {
                    return -2;
                }
            }

            icon_x = slot_x + (dock_icon_slot_px() - draw) / 2;
            icon_y = slot_y + (dock_icon_slot_px() - draw) / 2;

            dock_rect_cmd_t icon = {
                .x     = icon_x,
                .y     = icon_y,
                .w     = draw,
                .h     = draw,
                .color = p->secondary,
                .alpha = 255u,
                .kind  = 4u,
            };
            if (dock_clip_push(out, icon) != 0) {
                return -2;
            }

            if (it->active) {
                const int32_t pill_y =
                    slot_y + dock_icon_slot_px() +
                    ((int32_t)MDE_DOCK_INDICATOR_STRIP_PX - MDE_DOCK_ACTIVE_PILL_H) / 2;
                dock_rect_cmd_t dot = {
                    .x     = slot_x + dock_icon_slot_px() / 2 - MDE_DOCK_ACTIVE_PILL_W / 2,
                    .y     = pill_y,
                    .w     = MDE_DOCK_ACTIVE_PILL_W,
                    .h     = MDE_DOCK_ACTIVE_PILL_H,
                    .color = style->accent_glass,
                    .alpha = style->accent_alpha,
                    .kind  = 2u,
                };
                if (dock_clip_push(out, dot) != 0) {
                    return -2;
                }
            } else if (it->running) {
                const int32_t dot_y =
                    slot_y + dock_icon_slot_px() +
                    ((int32_t)MDE_DOCK_INDICATOR_STRIP_PX - MDE_DOCK_RUNNING_DOT) / 2;
                dock_rect_cmd_t dot = {
                    .x     = slot_x + dock_icon_slot_px() / 2 - MDE_DOCK_RUNNING_DOT / 2,
                    .y     = dot_y,
                    .w     = MDE_DOCK_RUNNING_DOT,
                    .h     = MDE_DOCK_RUNNING_DOT,
                    .color = style->border,
                    .alpha = 180u,
                    .kind  = 6u,
                };
                if (dock_clip_push(out, dot) != 0) {
                    return -2;
                }
            }

            if (g_labels_visible && MDE_DOCK_LABEL_BAND_PX > 0u) {
                dock_rect_cmd_t label_slot = {
                    .x     = slot_x,
                    .y     = slot_y + dock_icon_slot_px() + (int32_t)MDE_DOCK_INDICATOR_STRIP_PX,
                    .w     = dock_icon_slot_px(),
                    .h     = (int32_t)MDE_DOCK_LABEL_BAND_PX,
                    .color = style->fill,
                    .alpha = 0u,
                    .kind  = 5u,
                };
                if (dock_clip_push(out, label_slot) != 0) {
                    return -2;
                }
            }
        }
    }

    return 0;
}
/*
 * MDE desktop — menú contextual Liquid Glass (SA-22 / LG-06).
 */

#include <stddef.h>
#include <stdint.h>

#include "../theme/controls.h"
#include "../theme/glass.h"
#include "../theme/theme.h"

void mde_anim_tick(uint64_t now_ms);
void mde_anim_ctxmenu_enter(uint64_t now_ms);
void mde_anim_ctxmenu_dismiss(uint64_t now_ms);
void mde_anim_overlay_reset_closed(uint32_t fade_ch, uint32_t scale_ch);
int32_t mde_anim_ctxmenu_opacity(void);
int32_t mde_anim_ctxmenu_scale(void);

#define MDE_ANIM_CH_CTXMENU_FADE  60u
#define MDE_ANIM_CH_CTXMENU_SCALE 61u

#define MDE_CTXMENU_MAX_ITEMS       16u
#define MDE_CTXMENU_ITEM_HEIGHT     28u
#define MDE_CTXMENU_PADDING_X       12u
#define MDE_CTXMENU_SHORTCUT_PAD    48u
#define MDE_CTXMENU_MIN_WIDTH       160u
#define MDE_CTXMENU_BORDER          1u
#define MDE_CTXMENU_RADIUS          MDE_CTRL_GLASS_MENU_RADIUS
#define MDE_CTXMENU_SEPARATOR_H     9u

#define MDE_CTX_ACTION_NONE         0u
#define MDE_CTX_ACTION_OPEN         1u
#define MDE_CTX_ACTION_SEPARATOR    255u

typedef struct {
    uint8_t     action_id;
    const char *label;
} mde_ctxmenu_src_t;

typedef struct {
    uint8_t     action_id;
    const char *label;
    const char *shortcut;
} mde_ctxmenu_item_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} mde_rect_t;

static mde_ctxmenu_item_t g_items[MDE_CTXMENU_MAX_ITEMS];
static uint32_t           g_item_count;
static mde_rect_t         g_menu;
static int                g_visible;
static int32_t            g_hover_index;
static int32_t            g_pressed_index;
static int                g_anim_armed;

static mde_glass_menu_style g_glass_menu;

static uint32_t mde_ctx_label_width_estimate(const char *label)
{
    if (label == NULL) {
        return 0;
    }
    uint32_t n = 0;
    while (label[n] != '\0') {
        n++;
    }
    return n * 8u;
}

static void mde_ctx_split_label_shortcut(const char *src,
                                         const char **label_out,
                                         const char **shortcut_out)
{
    const char *tab;
    static const char empty = '\0';

    if (label_out == NULL || shortcut_out == NULL) {
        return;
    }
    if (src == NULL) {
        *label_out    = &empty;
        *shortcut_out = &empty;
        return;
    }

    tab = src;
    while (*tab != '\0' && *tab != '\t') {
        tab++;
    }
    if (*tab == '\t') {
        *label_out    = src;
        *shortcut_out = tab + 1;
    } else {
        *label_out    = src;
        *shortcut_out = &empty;
    }
}

static void mde_ctx_relayout(void)
{
    uint32_t max_label = 0;
    uint32_t max_short = 0;

    for (uint32_t i = 0; i < g_item_count; i++) {
        if (g_items[i].action_id == MDE_CTX_ACTION_SEPARATOR) {
            continue;
        }
        {
            uint32_t w = mde_ctx_label_width_estimate(g_items[i].label);
            if (w > max_label) {
                max_label = w;
            }
        }
        {
            uint32_t w = mde_ctx_label_width_estimate(g_items[i].shortcut);
            if (w > max_short) {
                max_short = w;
            }
        }
    }

    uint32_t inner_w = max_label + (MDE_CTXMENU_PADDING_X * 2u);
    if (max_short > 0) {
        inner_w += max_short + MDE_CTXMENU_SHORTCUT_PAD;
    }
    if (inner_w < MDE_CTXMENU_MIN_WIDTH) {
        inner_w = MDE_CTXMENU_MIN_WIDTH;
    }

    uint32_t total_h = MDE_CTXMENU_BORDER * 2u;
    for (uint32_t i = 0; i < g_item_count; i++) {
        if (g_items[i].action_id == MDE_CTX_ACTION_SEPARATOR) {
            total_h += MDE_CTXMENU_SEPARATOR_H;
        } else {
            total_h += MDE_CTXMENU_ITEM_HEIGHT;
        }
    }

    g_menu.w = (int32_t)(inner_w + MDE_CTXMENU_BORDER * 2u);
    g_menu.h = (int32_t)total_h;
}

void mde_ctxmenu_init(void)
{
    g_item_count    = 0;
    g_visible       = 0;
    g_hover_index   = -1;
    g_pressed_index = -1;
    g_anim_armed    = 0;
    g_menu.x        = 0;
    g_menu.y        = 0;
    g_menu.w        = 0;
    g_menu.h        = 0;

    mde_glass_menu_style_light(&g_glass_menu);
    mde_anim_overlay_reset_closed(MDE_ANIM_CH_CTXMENU_FADE, MDE_ANIM_CH_CTXMENU_SCALE);
}

void mde_ctxmenu_close(void)
{
    g_visible       = 0;
    g_hover_index   = -1;
    g_pressed_index = -1;
    g_item_count    = 0;
    g_anim_armed    = 0;
    mde_anim_overlay_reset_closed(MDE_ANIM_CH_CTXMENU_FADE, MDE_ANIM_CH_CTXMENU_SCALE);
}

int mde_ctxmenu_is_open(void)
{
    return g_visible;
}

void mde_ctxmenu_tick(uint64_t now_ms)
{
    if (!g_visible) {
        return;
    }

    if (g_anim_armed) {
        mde_anim_ctxmenu_enter(now_ms);
        g_anim_armed = 0;
    }

    (void)mde_anim_tick(now_ms);
}

static int mde_ctx_point_in_menu(int32_t px, int32_t py)
{
    return px >= g_menu.x && py >= g_menu.y &&
           px < (g_menu.x + g_menu.w) && py < (g_menu.y + g_menu.h);
}

static int32_t mde_ctx_item_at(int32_t px, int32_t py)
{
    if (!mde_ctx_point_in_menu(px, py)) {
        return -1;
    }

    int32_t local_y = py - g_menu.y - (int32_t)MDE_CTXMENU_BORDER;
    if (local_y < 0) {
        return -1;
    }

    for (uint32_t i = 0; i < g_item_count; i++) {
        int32_t row_h;
        if (g_items[i].action_id == MDE_CTX_ACTION_SEPARATOR) {
            row_h = (int32_t)MDE_CTXMENU_SEPARATOR_H;
        } else {
            row_h = (int32_t)MDE_CTXMENU_ITEM_HEIGHT;
        }

        if (local_y < row_h) {
            if (g_items[i].action_id == MDE_CTX_ACTION_SEPARATOR) {
                return -1;
            }
            return (int32_t)i;
        }
        local_y -= row_h;
    }

    return -1;
}

int mde_ctxmenu_open_at(int32_t screen_x, int32_t screen_y,
                        const mde_ctxmenu_src_t *items, uint32_t count)
{
    if (items == NULL || count == 0 || count > MDE_CTXMENU_MAX_ITEMS) {
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
        const char *label;
        const char *shortcut;

        g_items[i].action_id = items[i].action_id;
        mde_ctx_split_label_shortcut(items[i].label, &label, &shortcut);
        g_items[i].label    = label;
        g_items[i].shortcut = shortcut;
    }
    g_item_count = count;

    mde_ctx_relayout();

    g_menu.x = screen_x;
    g_menu.y = screen_y;
    g_visible       = 1;
    g_hover_index   = -1;
    g_pressed_index = -1;
    g_anim_armed    = 1;
    mde_anim_overlay_reset_closed(MDE_ANIM_CH_CTXMENU_FADE, MDE_ANIM_CH_CTXMENU_SCALE);
    return 0;
}

void mde_ctxmenu_pointer(int32_t x, int32_t y, int pressed, int32_t screen_w, int32_t screen_h)
{
    if (!g_visible) {
        return;
    }

    if (screen_w > 0 && screen_h > 0) {
        if (g_menu.x + g_menu.w > screen_w) {
            g_menu.x = screen_w - g_menu.w;
        }
        if (g_menu.y + g_menu.h > screen_h) {
            g_menu.y = screen_h - g_menu.h;
        }
        if (g_menu.x < 0) {
            g_menu.x = 0;
        }
        if (g_menu.y < 0) {
            g_menu.y = 0;
        }
    }

    int32_t idx = mde_ctx_item_at(x, y);

    if (pressed) {
        g_pressed_index = idx;
    } else if (g_pressed_index >= 0 && g_pressed_index == idx && idx >= 0) {
        g_pressed_index = -1;
    }

    g_hover_index = idx;
}

int32_t mde_ctxmenu_hovered_index(void)
{
    return g_hover_index;
}

uint8_t mde_ctxmenu_take_action(int32_t item_index)
{
    if (item_index < 0 || (uint32_t)item_index >= g_item_count) {
        return MDE_CTX_ACTION_NONE;
    }
    return g_items[item_index].action_id;
}

void mde_ctxmenu_bounds(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    if (x != NULL) {
        *x = g_menu.x;
    }
    if (y != NULL) {
        *y = g_menu.y;
    }
    if (w != NULL) {
        *w = g_menu.w;
    }
    if (h != NULL) {
        *h = g_menu.h;
    }
}

const mde_palette *mde_ctxmenu_theme(void)
{
    return theme_palette(theme_get_default());
}

uint32_t mde_ctxmenu_item_height(void)
{
    return MDE_CTXMENU_ITEM_HEIGHT;
}

uint32_t mde_ctxmenu_item_count(void)
{
    if (!g_visible) {
        return 0;
    }
    return g_item_count;
}

const char *mde_ctxmenu_item_label(uint32_t index)
{
    if (!g_visible || index >= g_item_count) {
        return NULL;
    }
    return g_items[index].label;
}

const char *mde_ctxmenu_item_shortcut(uint32_t index)
{
    if (!g_visible || index >= g_item_count) {
        return NULL;
    }
    if (g_items[index].action_id == MDE_CTX_ACTION_SEPARATOR) {
        return NULL;
    }
    return g_items[index].shortcut;
}

uint8_t mde_ctxmenu_item_action(uint32_t index)
{
    if (!g_visible || index >= g_item_count) {
        return MDE_CTX_ACTION_NONE;
    }
    return g_items[index].action_id;
}

int mde_ctxmenu_item_is_separator(uint32_t index)
{
    if (!g_visible || index >= g_item_count) {
        return 0;
    }
    return g_items[index].action_id == MDE_CTX_ACTION_SEPARATOR;
}

void mde_ctxmenu_glass_style(mde_glass_menu_style *out)
{
    if (out == NULL) {
        return;
    }
    *out = g_glass_menu;
}

uint32_t mde_ctxmenu_corner_radius(void)
{
    return (uint32_t)g_glass_menu.surface.corner_radius;
}

uint32_t mde_ctxmenu_blur_radius(void)
{
    return (uint32_t)g_glass_menu.surface.blur_radius;
}

uint32_t mde_ctxmenu_panel_fill(void)
{
    return g_glass_menu.surface.fill;
}

uint32_t mde_ctxmenu_panel_border_specular(void)
{
    return g_glass_menu.surface.border_specular;
}

uint32_t mde_ctxmenu_panel_border_base(void)
{
    return g_glass_menu.surface.border_base;
}

uint32_t mde_ctxmenu_item_hover_fill(void)
{
    return theme_palette(theme_get_default())->hover;
}

uint32_t mde_ctxmenu_shortcut_color(void)
{
    return theme_palette(theme_get_default())->muted;
}

uint32_t mde_ctxmenu_item_label_color(void)
{
    return theme_palette(theme_get_default())->text;
}

int32_t mde_ctxmenu_open_opacity(void)
{
    return mde_anim_ctxmenu_opacity();
}

int32_t mde_ctxmenu_open_scale(void)
{
    return mde_anim_ctxmenu_scale();
}

int32_t mde_ctxmenu_shortcut_x(uint32_t index, int32_t menu_x, int32_t menu_w)
{
    uint32_t sw;

    if (!g_visible || index >= g_item_count) {
        return menu_x + menu_w;
    }
    sw = mde_ctx_label_width_estimate(g_items[index].shortcut);
    return menu_x + menu_w - (int32_t)MDE_CTXMENU_PADDING_X - (int32_t)sw;
}
/* MettaOS MDE — topbar 28 px Liquid Glass Tahoe (LG-T04) */

#include "topbar.h"

#include "../gfx/font.h"
#include "../theme/controls.h"
#include "../theme/glass.h"
#include "../theme/theme.h"

#include <stddef.h>
#include <stdint.h>

#define MDE_TOPBAR_HEIGHT_PX       MDE_CTRL_GLASS_TOPBAR_HEIGHT
#define MDE_TOPBAR_ZONE_LOGO_W     88
#define MDE_TOPBAR_ZONE_APP_W      132
#define MDE_TOPBAR_ZONE_MENU_GAP   12
#define MDE_TOPBAR_MENU_TRACK_GAP  10
#define MDE_TOPBAR_RIGHT_PAD       16
#define MDE_TOPBAR_LEFT_PAD        16
#define MDE_TOPBAR_LOGO_MARK_W     18
#define MDE_TOPBAR_LOGO_MARK_H     14
#define MDE_TOPBAR_MENU_PILL_PAD_X 6
#define MDE_TOPBAR_MENU_PILL_H     20
#define MDE_TOPBAR_SEP_INSET_Y     7

#define TOPBAR_CMD_GLASS_PANEL     0u
#define TOPBAR_CMD_BORDER_RESERVED 1u
#define TOPBAR_CMD_SEPARATOR       2u
#define TOPBAR_CMD_LOGO_MARK       3u
#define TOPBAR_CMD_MENU_HOVER      4u
#define TOPBAR_CMD_SPECULAR_TOP    5u
#define TOPBAR_CMD_CHROME_RIGHT    6u
#define TOPBAR_CMD_SPECULAR_BOTTOM 7u

static mde_bar_style              g_topbar_bar;
static mde_glass_surface_style    g_topbar_glass_surface;
static topbar_glass_style_t       g_topbar_glass;
static topbar_layout_t            g_topbar_layout;
static int32_t                    g_viewport_w;
static char                       g_active_app[48];
static char                       g_clock_hhmm[6];
static topbar_menu_t              g_menus[MDE_TOPBAR_MAX_MENUS];
static uint32_t                   g_menu_count;
static int32_t                    g_hovered_menu;

static const topbar_menu_t k_default_menus[] = {
    { "Archivo" },
    { "Editar" },
    { "Ventana" },
    { "Ayuda" },
};

static uint8_t topbar_argb_alpha(uint32_t argb)
{
    return (uint8_t)((argb >> 24) & 0xffu);
}

static uint32_t topbar_argb_rgb(uint32_t argb)
{
    return argb & 0x00ffffffu;
}

static void topbar_reset_menus(void)
{
    g_menu_count = 0u;
    for (uint32_t i = 0u; i < MDE_TOPBAR_MAX_MENUS; ++i) {
        if (i < (sizeof(k_default_menus) / sizeof(k_default_menus[0]))) {
            g_menus[g_menu_count].label = k_default_menus[i].label;
            ++g_menu_count;
        }
    }
}

static void topbar_clock_format(uint8_t hour, uint8_t minute)
{
    if (hour > 23u) {
        hour = 0u;
    }
    if (minute > 59u) {
        minute = 0u;
    }
    g_clock_hhmm[0] = (char)('0' + (hour / 10u));
    g_clock_hhmm[1] = (char)('0' + (hour % 10u));
    g_clock_hhmm[2] = ':';
    g_clock_hhmm[3] = (char)('0' + (minute / 10u));
    g_clock_hhmm[4] = (char)('0' + (minute % 10u));
    g_clock_hhmm[5] = '\0';
}

static void topbar_glass_tokens_apply(void)
{
    const mde_glass_tokens *tok = &mde_light_glass;

    mde_bar_glass_style_light(&g_topbar_bar, &g_topbar_glass_surface);

    g_topbar_glass.fill                 = topbar_argb_rgb(g_topbar_glass_surface.fill);
    g_topbar_glass.border_base          = topbar_argb_rgb(g_topbar_glass_surface.border_base);
    g_topbar_glass.border_specular      = topbar_argb_rgb(g_topbar_glass_surface.border_specular);
    g_topbar_glass.specular_top         = topbar_argb_rgb(g_topbar_glass_surface.specular_top);
    g_topbar_glass.menu_pill            = topbar_argb_rgb(MDE_GLASS_FILL_CHROME);
    g_topbar_glass.menu_pill_hover      = topbar_argb_rgb(tok->accent_hover);
    g_topbar_glass.blur_radius          = g_topbar_glass_surface.blur_radius;
    g_topbar_glass.border_width         = g_topbar_glass_surface.border_width;
    g_topbar_glass.specular_top_height  = g_topbar_glass_surface.specular_top_height;
    g_topbar_glass.capsule_margin_x     = 0u;
    g_topbar_glass.corner_radius        = 0u;
}

static int topbar_clip_push(topbar_draw_list_t *list, topbar_rect_cmd_t cmd)
{
    if (list == NULL || list->count >= MDE_TOPBAR_MAX_DRAW_CMDS) {
        return -1;
    }
    list->cmds[list->count++] = cmd;
    return 0;
}

static int32_t topbar_menu_pill_width(const char *label)
{
    int32_t text_w;

    if (label == NULL) {
        return 36;
    }
    text_w = font_string_advance(label);
    return text_w + MDE_TOPBAR_MENU_PILL_PAD_X * 2;
}

void topbar_init(void)
{
    g_viewport_w      = 0;
    g_hovered_menu    = -1;
    g_active_app[0]   = '\0';
    topbar_glass_tokens_apply();
    topbar_clock_format(0u, 0u);
    topbar_reset_menus();
    g_topbar_layout.x = 0;
    g_topbar_layout.y = 0;
    g_topbar_layout.w = 0;
    g_topbar_layout.h = (int32_t)MDE_TOPBAR_HEIGHT_PX;
}

void topbar_set_viewport(int32_t width)
{
    if (width < 0) {
        width = 0;
    }
    g_viewport_w = width;
}

void topbar_set_active_app(const char *name)
{
    if (name == NULL) {
        g_active_app[0] = '\0';
        return;
    }
    size_t i = 0u;
    while (name[i] != '\0' && i < (sizeof(g_active_app) - 1u)) {
        g_active_app[i] = name[i];
        ++i;
    }
    g_active_app[i] = '\0';
}

void topbar_set_clock(uint8_t hour, uint8_t minute)
{
    topbar_clock_format(hour, minute);
}

void topbar_set_hovered_menu(int32_t menu_index)
{
    if (menu_index < 0 || (uint32_t)menu_index >= g_menu_count) {
        g_hovered_menu = -1;
        return;
    }
    g_hovered_menu = menu_index;
}

int32_t topbar_hovered_menu(void)
{
    return g_hovered_menu;
}

uint8_t topbar_get_material(void)
{
    return TOPBAR_MATERIAL_GLASS;
}

const topbar_glass_style_t *topbar_get_glass_style(void)
{
    return &g_topbar_glass;
}

int topbar_shows_calendar(void)
{
    return MDE_TOPBAR_SHOW_CALENDAR;
}

const char *topbar_clock_text(void)
{
    return g_clock_hhmm;
}

const char *topbar_active_app(void)
{
    return g_active_app;
}

uint32_t topbar_menu_count(void)
{
    return g_menu_count;
}

const char *topbar_menu_label(uint32_t index)
{
    if (index >= g_menu_count) {
        return NULL;
    }
    return g_menus[index].label;
}

const topbar_layout_t *topbar_layout_compute(void)
{
    const int32_t clock_text_w = font_string_advance(g_clock_hhmm);
    int32_t after_logo;

    g_topbar_layout.h = (int32_t)g_topbar_bar.height;
    g_topbar_layout.x = 0;
    g_topbar_layout.y = 0;
    g_topbar_layout.w = g_viewport_w > 0 ? g_viewport_w : 0;

    g_topbar_layout.logo_x = g_topbar_layout.x + MDE_TOPBAR_LEFT_PAD;
    after_logo             = g_topbar_layout.logo_x + MDE_TOPBAR_ZONE_LOGO_W;
    g_topbar_layout.app_x  = after_logo;

    if (g_active_app[0] != '\0') {
        g_topbar_layout.menus_x =
            after_logo + MDE_TOPBAR_ZONE_APP_W + MDE_TOPBAR_ZONE_MENU_GAP;
    } else {
        g_topbar_layout.menus_x = after_logo + MDE_TOPBAR_ZONE_MENU_GAP;
    }

    g_topbar_layout.clock_x =
        g_topbar_layout.x + g_topbar_layout.w - MDE_TOPBAR_RIGHT_PAD - clock_text_w;

    /* Oculta “Cfg” legacy del compositor (LG-T04: reloj sin clutter). */
    g_topbar_layout.config_x = -256;

    return &g_topbar_layout;
}

int topbar_build_draw_list(topbar_draw_list_t *out)
{
    const topbar_layout_t *layout;
    const mde_palette *p;
    int32_t menu_pen_x;
    uint32_t mi;
    const uint8_t border_w =
        g_topbar_glass.border_width > 0u ? g_topbar_glass.border_width : 1u;

    if (out == NULL) {
        return -1;
    }
    out->count = 0u;

    layout = topbar_layout_compute();
    p      = theme_palette(theme_get_default());

    {
        topbar_rect_cmd_t background = {
            .x     = layout->x,
            .y     = layout->y,
            .w     = layout->w,
            .h     = layout->h,
            .color = g_topbar_glass.fill,
            .kind  = TOPBAR_CMD_GLASS_PANEL,
        };
        if (topbar_clip_push(out, background) != 0) {
            return -2;
        }
    }

    {
        topbar_rect_cmd_t spec = {
            .x     = layout->x,
            .y     = layout->y,
            .w     = layout->w,
            .h     = (int32_t)g_topbar_glass.specular_top_height,
            .color = g_topbar_glass.specular_top,
            .kind  = TOPBAR_CMD_SPECULAR_TOP,
        };
        if (topbar_clip_push(out, spec) != 0) {
            return -2;
        }
    }

    {
        topbar_rect_cmd_t spec_bottom = {
            .x     = layout->x,
            .y     = layout->y + layout->h - (int32_t)border_w,
            .w     = layout->w,
            .h     = (int32_t)border_w,
            .color = g_topbar_glass.border_specular,
            .kind  = TOPBAR_CMD_SPECULAR_BOTTOM,
        };
        if (topbar_clip_push(out, spec_bottom) != 0) {
            return -2;
        }
    }

    menu_pen_x = layout->menus_x;
    for (mi = 0u; mi < g_menu_count; ++mi) {
        const char *lab     = g_menus[mi].label;
        const int32_t pill_w = topbar_menu_pill_width(lab);
        const int32_t pill_y =
            layout->y + (layout->h - MDE_TOPBAR_MENU_PILL_H) / 2;

        if (g_hovered_menu >= 0 && (int32_t)mi == g_hovered_menu) {
            topbar_rect_cmd_t pill = {
                .x     = menu_pen_x - MDE_TOPBAR_MENU_PILL_PAD_X,
                .y     = pill_y,
                .w     = pill_w,
                .h     = MDE_TOPBAR_MENU_PILL_H,
                .color = g_topbar_glass.menu_pill_hover,
                .kind  = TOPBAR_CMD_MENU_HOVER,
            };
            if (topbar_clip_push(out, pill) != 0) {
                return -2;
            }
        }

        menu_pen_x += font_string_advance(lab) + MDE_TOPBAR_MENU_TRACK_GAP;
    }

    {
        topbar_rect_cmd_t logo_mark = {
            .x     = layout->logo_x,
            .y     = layout->y + (layout->h - MDE_TOPBAR_LOGO_MARK_H) / 2,
            .w     = MDE_TOPBAR_LOGO_MARK_W,
            .h     = MDE_TOPBAR_LOGO_MARK_H,
            .color = p->accent,
            .kind  = TOPBAR_CMD_LOGO_MARK,
        };
        if (topbar_clip_push(out, logo_mark) != 0) {
            return -2;
        }
    }

    if (g_active_app[0] != '\0') {
        topbar_rect_cmd_t app_sep = {
            .x     = layout->menus_x - 8,
            .y     = layout->y + MDE_TOPBAR_SEP_INSET_Y,
            .w     = 1,
            .h     = layout->h - MDE_TOPBAR_SEP_INSET_Y * 2,
            .color = topbar_argb_rgb(MDE_GLASS_BORDER_BASE),
            .kind  = TOPBAR_CMD_SEPARATOR,
        };
        if (topbar_clip_push(out, app_sep) != 0) {
            return -2;
        }
    }

#if MDE_TOPBAR_SHOW_CALENDAR
#error "LG-T04: la topbar no debe mostrar calendario"
#endif

    (void)TOPBAR_CMD_BORDER_RESERVED;
    (void)TOPBAR_CMD_CHROME_RIGHT;
    (void)topbar_argb_alpha(mde_light_glass.fill_secondary);
    return 0;
}
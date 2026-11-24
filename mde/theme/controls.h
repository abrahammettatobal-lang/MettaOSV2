#ifndef MDE_CONTROLS_H
#define MDE_CONTROLS_H

#include "glass.h"
#include "light.h"

/*
 * Métricas MDE Light + Liquid Glass (Tahoe) — compactos, sin “web cards”.
 * Flat legacy conservado; chrome glass usa radios/altura dedicados.
 */

#define MDE_CTRL_BAR_HEIGHT          28
#define MDE_CTRL_DOCK_MARGIN_BOTTOM  12
#define MDE_CTRL_DOCK_PADDING        8

/* Liquid Glass — radios Tahoe (dock 22–28, ventana 12–16, menú 10–12). */
#define MDE_CTRL_GLASS_TOPBAR_HEIGHT 28
#define MDE_CTRL_GLASS_DOCK_RADIUS   24
#define MDE_CTRL_GLASS_WINDOW_RADIUS 14
#define MDE_CTRL_GLASS_MENU_RADIUS   11
#define MDE_CTRL_DOCK_RADIUS         MDE_CTRL_GLASS_DOCK_RADIUS
#define MDE_CTRL_GLASS_DOCK_BLUR     MDE_GLASS_BLUR_HEAVY
#define MDE_CTRL_GLASS_TOPBAR_BLUR   MDE_GLASS_BLUR_LIGHT
#define MDE_CTRL_GLASS_WINDOW_BLUR   MDE_GLASS_BLUR_MEDIUM
#define MDE_CTRL_GLASS_MENU_BLUR     MDE_GLASS_BLUR_LIGHT

#define MDE_CTRL_BUTTON_HEIGHT       28
#define MDE_CTRL_BUTTON_MIN_WIDTH    64
#define MDE_CTRL_BUTTON_RADIUS       6
#define MDE_CTRL_BUTTON_PADDING_X    12
#define MDE_CTRL_BUTTON_BORDER       1

#define MDE_CTRL_WINDOW_RADIUS       10
#define MDE_CTRL_WINDOW_BORDER       1
#define MDE_CTRL_WINDOW_TITLE_HEIGHT 28
#define MDE_CTRL_WINDOW_SHADOW_ALPHA 24 /* sombra sutil, no card pesada */

typedef struct mde_button_style {
    uint32_t fill;
    uint32_t fill_hover;
    uint32_t fill_pressed;
    uint32_t border;
    uint32_t label;
    uint32_t label_disabled;
    uint16_t height;
    uint16_t radius;
    uint16_t padding_x;
    uint8_t  border_width;
} mde_button_style;

typedef struct mde_bar_style {
    uint32_t background;
    uint32_t border_bottom;
    uint32_t label;
    uint32_t icon;
    uint16_t height;
    uint8_t  border_width;
} mde_bar_style;

typedef struct mde_window_style {
    uint32_t surface;
    uint32_t titlebar;
    uint32_t title_text;
    uint32_t border;
    uint32_t client_bg;
    uint16_t corner_radius;
    uint16_t title_height;
    uint8_t  border_width;
} mde_window_style;

typedef struct mde_glass_surface_style {
    uint32_t fill;
    uint32_t border_specular;
    uint32_t border_base;
    uint32_t specular_top;
    uint16_t corner_radius;
    uint8_t  border_width;
    uint8_t  specular_top_height;
    uint8_t  blur_radius;
} mde_glass_surface_style;

typedef struct mde_glass_dock_style {
    mde_glass_surface_style surface;
    uint16_t padding;
    uint16_t margin_bottom;
} mde_glass_dock_style;

typedef struct mde_glass_menu_style {
    mde_glass_surface_style surface;
    uint16_t item_height;
    uint16_t padding_x;
} mde_glass_menu_style;

static inline void mde_button_style_light(mde_button_style *s)
{
    const mde_palette *p = &mde_light_palette;

    s->fill           = p->surface;
    s->fill_hover     = p->hover;
    s->fill_pressed   = p->selection;
    s->border         = p->border;
    s->label          = p->text;
    s->label_disabled = p->muted;
    s->height         = MDE_CTRL_BUTTON_HEIGHT;
    s->radius         = MDE_CTRL_BUTTON_RADIUS;
    s->padding_x      = MDE_CTRL_BUTTON_PADDING_X;
    s->border_width   = MDE_CTRL_BUTTON_BORDER;
}

static inline void mde_button_style_primary_light(mde_button_style *s)
{
    const mde_palette *p = &mde_light_palette;

    mde_button_style_light(s);
    s->fill         = p->accent;
    s->fill_hover   = MDE_COLOR_RGB(0x2B6BC4u); /* accent ligeramente más oscuro */
    s->fill_pressed = MDE_COLOR_RGB(0x245FAFu);
    s->border       = p->accent;
    s->label        = MDE_COLOR_RGB(0xFFFFFFu);
}

static inline void mde_bar_style_light(mde_bar_style *s)
{
    const mde_palette *p = &mde_light_palette;

    s->background     = p->surface;
    s->border_bottom  = p->border;
    s->label          = p->text;
    s->icon           = p->muted;
    s->height         = MDE_CTRL_BAR_HEIGHT;
    s->border_width   = 1;
}

static inline void mde_window_style_light(mde_window_style *s)
{
    const mde_palette *p = &mde_light_palette;

    s->surface       = p->surface;
    s->titlebar      = p->secondary;
    s->title_text    = p->text;
    s->border        = p->border;
    s->client_bg     = p->surface;
    s->corner_radius = MDE_CTRL_WINDOW_RADIUS;
    s->title_height  = MDE_CTRL_WINDOW_TITLE_HEIGHT;
    s->border_width  = MDE_CTRL_WINDOW_BORDER;
}

static inline void mde_glass_surface_fill(
    mde_glass_surface_style *s, const mde_glass_tokens *g)
{
    s->fill                = g->fill_primary;
    s->border_specular     = g->border_specular;
    s->border_base         = g->border_base;
    s->specular_top        = g->specular_top;
    s->border_width        = g->border_width;
    s->specular_top_height = g->specular_top_height;
}

static inline void mde_glass_topbar_style_light(mde_glass_surface_style *s)
{
    const mde_glass_tokens *g = &mde_light_glass;

    mde_glass_surface_fill(s, g);
    s->fill          = g->fill_secondary;
    s->corner_radius = 0;
    s->blur_radius   = (uint8_t)MDE_CTRL_GLASS_TOPBAR_BLUR;
}

static inline void mde_glass_dock_style_light(mde_glass_dock_style *s)
{
    const mde_glass_tokens *g = &mde_light_glass;

    mde_glass_surface_fill(&s->surface, g);
    s->surface.fill          = g->fill_primary;
    s->surface.corner_radius = MDE_CTRL_GLASS_DOCK_RADIUS;
    s->surface.blur_radius   = (uint8_t)MDE_CTRL_GLASS_DOCK_BLUR;
    s->padding               = MDE_CTRL_DOCK_PADDING;
    s->margin_bottom         = MDE_CTRL_DOCK_MARGIN_BOTTOM;
}

static inline void mde_glass_window_style_light(mde_glass_surface_style *s)
{
    const mde_glass_tokens *g = &mde_light_glass;

    mde_glass_surface_fill(s, g);
    s->corner_radius = MDE_CTRL_GLASS_WINDOW_RADIUS;
    s->blur_radius   = (uint8_t)MDE_CTRL_GLASS_WINDOW_BLUR;
}

static inline void mde_glass_menu_style_light(mde_glass_menu_style *s)
{
    const mde_glass_tokens *g = &mde_light_glass;

    mde_glass_surface_fill(&s->surface, g);
    s->surface.corner_radius = MDE_CTRL_GLASS_MENU_RADIUS;
    s->surface.blur_radius   = (uint8_t)MDE_CTRL_GLASS_MENU_BLUR;
    s->item_height           = MDE_CTRL_BUTTON_HEIGHT;
    s->padding_x             = MDE_CTRL_BUTTON_PADDING_X;
}

static inline void mde_bar_glass_style_light(mde_bar_style *flat, mde_glass_surface_style *glass)
{
    mde_bar_style_light(flat);
    flat->height = MDE_CTRL_GLASS_TOPBAR_HEIGHT;
    mde_glass_topbar_style_light(glass);
    flat->background    = glass->fill;
    flat->border_bottom = glass->border_base;
}

static inline void mde_window_glass_style_light(mde_window_style *flat, mde_glass_surface_style *glass)
{
    const mde_palette *p = &mde_light_palette;

    mde_window_style_light(flat);
    mde_glass_window_style_light(glass);
    flat->surface       = glass->fill;
    flat->titlebar      = glass->fill;
    flat->border        = glass->border_base;
    flat->client_bg     = p->surface;
    flat->corner_radius = glass->corner_radius;
    flat->title_height  = MDE_CTRL_GLASS_TOPBAR_HEIGHT;
}

#endif /* MDE_CONTROLS_H */
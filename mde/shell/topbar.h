#ifndef MDE_SHELL_TOPBAR_H
#define MDE_SHELL_TOPBAR_H

#include <stdint.h>

/* Material para el compositor (blur / capa glass). */
#define TOPBAR_MATERIAL_FLAT  0u
#define TOPBAR_MATERIAL_GLASS 1u

#define MDE_TOPBAR_MAX_DRAW_CMDS 24u
#define MDE_TOPBAR_MAX_MENUS     4u

#ifndef MDE_TOPBAR_HEIGHT_PX
/* Fallback si controls.h no está incluido antes en tests mínimos. */
#define MDE_TOPBAR_HEIGHT_PX 28
#endif

/* LG-T04 Tahoe: barra superior full-width, 28 px. */
#define MDE_TOPBAR_LAYOUT_HEIGHT MDE_TOPBAR_HEIGHT_PX

/* SA-20 / LG-04 / LG-T04: reloj HH:MM únicamente; sin calendario. */
#define MDE_TOPBAR_SHOW_CALENDAR 0

typedef struct topbar_menu {
    const char *label;
} topbar_menu_t;

typedef struct topbar_layout {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t logo_x;
    int32_t app_x;
    int32_t menus_x;
    int32_t clock_x;
    int32_t config_x; /* reservado (compositor legacy); oculto si < 0 */
} topbar_layout_t;

/*
 * kind (contrato compositor_paint.c / COMP_SHELL_CMD_*):
 *  0 = panel glass barra (único que dispara blur; debe coincidir con layout)
 *  1 = borde inferior specular — reservado; no se pinta (glass aplica arista)
 *  2 = separador vertical discreto
 *  3 = marca logo Metta (acento)
 *  4 = hover pill menú (glass chrome)
 *  5 = franja specular superior 1 px
 *  6 = chrome auxiliar derecho (reservado; LG-T04 sin clutter)
 *  7 = filo specular inferior 1 px (Tahoe)
 */
typedef struct topbar_rect_cmd {
    int32_t  x;
    int32_t  y;
    int32_t  w;
    int32_t  h;
    uint32_t color;
    uint8_t  kind;
} topbar_rect_cmd_t;

typedef struct topbar_draw_list {
    topbar_rect_cmd_t cmds[MDE_TOPBAR_MAX_DRAW_CMDS];
    uint32_t          count;
} topbar_draw_list_t;

typedef struct topbar_glass_style {
    uint32_t fill;
    uint32_t border_base;
    uint32_t border_specular;
    uint32_t specular_top;
    uint32_t menu_pill;
    uint32_t menu_pill_hover;
    uint8_t  blur_radius;
    uint8_t  border_width;
    uint8_t  specular_top_height;
    uint16_t capsule_margin_x; /* LG-T04: 0 = full-width */
    uint16_t corner_radius;
} topbar_glass_style_t;

void topbar_init(void);
void topbar_set_viewport(int32_t width);
void topbar_set_active_app(const char *name);
void topbar_set_clock(uint8_t hour, uint8_t minute);
void topbar_set_hovered_menu(int32_t menu_index);

int  topbar_shows_calendar(void);

const char *topbar_clock_text(void);
const char *topbar_active_app(void);

uint32_t    topbar_menu_count(void);
const char *topbar_menu_label(uint32_t index);
int32_t     topbar_hovered_menu(void);

uint8_t                    topbar_get_material(void);
const topbar_glass_style_t *topbar_get_glass_style(void);

const topbar_layout_t *topbar_layout_compute(void);
int                    topbar_build_draw_list(topbar_draw_list_t *out);

#endif /* MDE_SHELL_TOPBAR_H */
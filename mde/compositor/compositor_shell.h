/*
 * Contrato de enlazado shell MDE (SA-20) — declaraciones para el compositor.
 * Tipos alineados con mde/shell/dock.c y topbar.c.
 */
#ifndef MDE_COMPOSITOR_SHELL_H
#define MDE_COMPOSITOR_SHELL_H

#include <stdint.h>

#include "../shell/dock.h"

#define MDE_TOPBAR_MAX_DRAW_CMDS 24u

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

typedef struct topbar_layout {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t logo_x;
    int32_t app_x;
    int32_t menus_x;
    int32_t clock_x;
    int32_t config_x;
} topbar_layout_t;

void topbar_init(void);
void topbar_set_viewport(int32_t width);
const topbar_layout_t *topbar_layout_compute(void);
int  topbar_build_draw_list(topbar_draw_list_t *out);

const char *topbar_clock_text(void);
const char *topbar_active_app(void);
uint32_t    topbar_menu_count(void);
const char *topbar_menu_label(uint32_t index);

#endif /* MDE_COMPOSITOR_SHELL_H */
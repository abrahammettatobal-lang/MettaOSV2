#ifndef MDE_SHELL_DOCK_H
#define MDE_SHELL_DOCK_H

#include <stdint.h>

/* Material para el compositor (blur / capa glass). */
#define DOCK_MATERIAL_FLAT  0u
#define DOCK_MATERIAL_GLASS 1u

/*
 * Métricas públicas — contrato compositor / interacción (LG-T03 Tahoe).
 * Íconos en rango 40–48 px; assets actuales 40×40, slot 44 px con aire interno.
 */
#define MDE_DOCK_ICON_PX            44u
#define MDE_DOCK_ICON_DRAW_PX       40u /* celda bitmap ícono (compositor) */
#define MDE_DOCK_ITEM_GAP_PX        10u
#define MDE_DOCK_INDICATOR_STRIP_PX 6u  /* espacio bajo ícono para punto/píldora */
#define MDE_DOCK_LABEL_BAND_PX      0u  /* etiquetas vía compositor; sin banda extra */

#define MDE_DOCK_MAX_DRAW_CMDS 96u

/*
 * kind:
 *  0 = panel / sombra
 *  1 = borde (contorno discreto)
 *  2 = indicador activo (azul glass)
 *  3 = franja specular superior
 *  4 = slot ícono / hover glass
 *  5 = reserva etiqueta (ancla layout; texto en compositor)
 *  6 = indicador «running» (punto tenue)
 */
typedef struct dock_rect_cmd {
    int32_t  x;
    int32_t  y;
    int32_t  w;
    int32_t  h;
    uint32_t color;
    uint8_t  alpha;
    uint8_t  kind;
} dock_rect_cmd_t;

typedef struct dock_draw_list {
    dock_rect_cmd_t cmds[MDE_DOCK_MAX_DRAW_CMDS];
    uint32_t        count;
} dock_draw_list_t;

typedef struct dock_layout {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t item_count;
    int32_t icon_size;   /* MDE_DOCK_ICON_PX */
    int32_t item_stride; /* icon + gap */
    int32_t icon_y;      /* fila de íconos dentro del panel */
} dock_layout_t;

typedef struct dock_style {
    uint32_t fill;
    uint8_t  fill_alpha;
    uint32_t border;
    uint32_t border_specular;
    uint32_t specular_top;
    uint32_t shadow;
    uint32_t accent_glass;
    uint8_t  accent_alpha;
    uint16_t radius;
    uint16_t padding;
    uint16_t margin_bottom;
    uint8_t  border_width;
    uint8_t  blur_radius;
    uint8_t  material;
} dock_style_t;

void dock_init(void);
void dock_set_viewport(int32_t width, int32_t height);

/** Registra un ítem; `running` marca app abierta (punto tenue). */
int  dock_item_add(const char *app_id, const char *label, int running);
void dock_clear_items(void);
void dock_set_active_app(const char *app_id);
void dock_set_hovered_index(int32_t index);

/** Actualiza estado «running» por `app_id` sin reordenar ítems. */
int dock_item_set_running(const char *app_id, int running);

uint32_t    dock_item_count(void);
const char *dock_item_label(uint32_t index);
const char *dock_item_app_id(uint32_t index);
int32_t     dock_hovered_index(void);
int32_t     dock_index_for_app_id(const char *app_id);
int         dock_item_is_active(uint32_t index);
int         dock_item_is_running(uint32_t index);

/**
 * Rectángulo del slot de ícono (incluye aire hasta la franja indicador).
 * Devuelve 0 si `index` válido.
 */
int dock_item_hit_rect(uint32_t index, int32_t *out_x, int32_t *out_y,
                       int32_t *out_w, int32_t *out_h);

/** Celda 40×40 para bitmap; centrada en el slot de 44 px. */
int dock_item_icon_draw_rect(uint32_t index, int32_t *out_x, int32_t *out_y,
                             int32_t *out_w, int32_t *out_h);

/** Ancla opcional de etiqueta (bajo indicador); útil si el compositor pinta texto. */
int dock_item_label_anchor(uint32_t index, int32_t *out_x, int32_t *out_y);

/** Etiquetas bajo íconos: 0 por defecto (prioriza íconos). */
void dock_set_labels_visible(int visible);
int  dock_labels_visible(void);

/** Índice de ítem bajo (x,y), o -1. */
int32_t dock_hit_test(int32_t x, int32_t y);

uint8_t            dock_get_material(void);
const dock_style_t *dock_get_style(void);

const dock_layout_t *dock_layout_compute(void);
int                  dock_build_draw_list(dock_draw_list_t *out);

#endif /* MDE_SHELL_DOCK_H */
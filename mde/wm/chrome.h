#ifndef MDE_WM_CHROME_H
#define MDE_WM_CHROME_H

#include "window.h"
#include "../theme/controls.h"
#include "../theme/glass.h"

#define MDE_WM_MAX_DRAW_CMDS       48u
#define MDE_WM_CTRL_SIZE           12u
#define MDE_WM_CTRL_GAP            8u
#define MDE_WM_CTRL_MARGIN_LEFT    12u
#define MDE_WM_SHADOW_PAD_GLASS    14 /* margen alrededor del marco para sombra soft */

#define MDE_WM_GRIP_SIZE           5u
#define MDE_WM_GRIP_INSET          4u

typedef enum {
    MDE_WM_CHROME_CMD_FILL = 0,
    MDE_WM_CHROME_CMD_BORDER,
    MDE_WM_CHROME_CMD_SHADOW,
    MDE_WM_CHROME_CMD_SPECULAR,
    MDE_WM_CHROME_CMD_CTRL,
    MDE_WM_CHROME_CMD_TITLE,
    MDE_WM_CHROME_CMD_GRIP,
} mde_wm_chrome_cmd_kind_t;

typedef struct mde_wm_chrome_rect_cmd {
    mde_rect_t                  rect;
    uint32_t                    color;
    uint8_t                     alpha;
    uint8_t                     kind; /* mde_wm_chrome_cmd_kind_t */
    uint8_t                     blur_radius;
    uint16_t                    corner_radius;
} mde_wm_chrome_rect_cmd_t;

typedef struct mde_wm_chrome_draw_list {
    mde_wm_chrome_rect_cmd_t    cmds[MDE_WM_MAX_DRAW_CMDS];
    uint32_t                    count;
} mde_wm_chrome_draw_list_t;

typedef enum {
    MDE_WM_CTRL_NONE = 0,
    MDE_WM_CTRL_CLOSE  = (1u << 0),
    MDE_WM_CTRL_MINIMIZE = (1u << 1),
    MDE_WM_CTRL_MAXIMIZE = (1u << 2),
} mde_wm_ctrl_id_t;

typedef struct mde_chrome_layer {
    mde_window_style            style;
    mde_glass_surface_style     glass;
    const mde_glass_tokens     *tokens;
    mde_rect_t                  shadow_offset;
    uint8_t                     shadow_alpha;
    uint32_t                    client_fill;   /* #FFFFFF sólido */
    uint32_t                    client_glass;  /* variante glass ligera */
    uint8_t                     client_use_glass; /* 0 = sólido */
} mde_chrome_layer_t;

void wm_chrome_init(void);
const mde_chrome_layer_t *wm_chrome_layer(void);

mde_rect_t wm_chrome_frame_rect(const mde_window_t *win);
mde_rect_t wm_chrome_titlebar_rect(const mde_window_t *win);
mde_rect_t wm_chrome_client_rect(const mde_window_t *win);
mde_rect_t wm_chrome_body_rect(const mde_window_t *win); /* título + cliente, sin sombra */
mde_rect_t wm_chrome_title_label_rect(const mde_window_t *win);

uint32_t wm_chrome_titlebar_color(const mde_window_t *win, uint32_t focused_id);
uint32_t wm_chrome_client_color(const mde_window_t *win, uint32_t focused_id);
uint32_t wm_chrome_title_text_color(const mde_window_t *win, uint32_t focused_id);
uint8_t  wm_chrome_titlebar_blur(const mde_window_t *win, uint32_t focused_id);
uint8_t  wm_chrome_frame_border_alpha(const mde_window_t *win, uint32_t focused_id);

mde_rect_t wm_chrome_control_rect(const mde_window_t *win, mde_wm_ctrl_id_t ctrl);
uint32_t   wm_chrome_control_color(mde_wm_ctrl_id_t ctrl, uint8_t hover,
                                   const mde_window_t *win, uint32_t focused_id);

/* Hit-test: un solo control bajo (px, py). */
mde_wm_ctrl_id_t wm_chrome_control_hit(const mde_window_t *win, int32_t px, int32_t py);

void wm_chrome_mark_dirty(mde_window_t *win);
void wm_chrome_invalidate_window(const mde_window_t *win);

uint32_t wm_chrome_collect_damage(const mde_window_t *win, uint32_t focused_id,
                                  mde_rect_t *out, uint32_t cap);

/*
 * Draw list (atrás → delante):
 *   sombras (×3, alpha atenuada si inactiva)
 *   titlebar glass + specular 1 px
 *   cliente #FFFFFF
 *   borde base + specular (alpha según foco)
 *   traffic lights izquierda (rojo / amarillo / verde)
 *   MDE_WM_CHROME_CMD_TITLE (ancla título centrado Tahoe)
 *   grips de resize discretos (si MDE_WIN_FLAG_RESIZABLE)
 */
int wm_chrome_build_draw_list(const mde_window_t *win, uint32_t focused_id,
                              mde_wm_chrome_draw_list_t *out);

/* Actualiza hover de controles en ventana (bitmask en win->chrome_ctrl_hover). */
int wm_chrome_pointer_hover(mde_window_t *win, int32_t px, int32_t py);

#endif /* MDE_WM_CHROME_H */
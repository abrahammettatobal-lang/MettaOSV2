#ifndef MDE_WM_WINDOW_H
#define MDE_WM_WINDOW_H

#include <stdint.h>

#define MDE_WM_MAX_WINDOWS   64u
#define MDE_WM_TITLE_MAX       63u

typedef struct mde_rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} mde_rect_t;

typedef enum {
    MDE_WIN_STATE_NORMAL = 0,
    MDE_WIN_STATE_MINIMIZED,
    MDE_WIN_STATE_MAXIMIZED,
} mde_win_state_t;

#define MDE_WIN_FLAG_RESIZABLE  (1u << 0)
#define MDE_WIN_FLAG_DECORATED  (1u << 1)
#define MDE_WIN_FLAG_MODAL      (1u << 2)

/*
 * bounds: origen en la esquina superior izquierda de la barra de título;
 * w = ancho del marco; h = alto del área cliente (sin título).
 */
typedef struct mde_window {
    uint32_t         id;
    char             title[MDE_WM_TITLE_MAX + 1u];
    mde_rect_t       bounds;
    mde_rect_t       restore_bounds;
    mde_win_state_t  state;
    uint32_t         flags;
    uint32_t         z_order;
    uint32_t         surface_token; /* token compositor; se conserva al mover */
    uint8_t          in_use;
    uint8_t          chrome_dirty;
    uint8_t          chrome_ctrl_hover; /* bitmask mde_wm_ctrl_id_t */
} mde_window_t;

typedef enum {
    MDE_WM_DRAG_NONE = 0,
    MDE_WM_DRAG_MOVE,
    MDE_WM_DRAG_RESIZE,
} mde_wm_drag_kind_t;

#define MDE_WM_RESIZE_N  (1u << 0)
#define MDE_WM_RESIZE_S  (1u << 1)
#define MDE_WM_RESIZE_E  (1u << 2)
#define MDE_WM_RESIZE_W  (1u << 3)

typedef struct mde_wm_interaction {
    uint32_t            active;
    uint32_t            window_id;
    mde_wm_drag_kind_t  kind;
    uint32_t            resize_edges;
    int32_t             pointer_x;
    int32_t             pointer_y;
    int32_t             last_pointer_x;
    int32_t             last_pointer_y;
    mde_rect_t          start_bounds;
} mde_wm_interaction_t;

/* Contrato mínimo con compositor (SA-15): reutilizar superficie en traslación. */
typedef struct mde_compositor_ops {
    void (*invalidate_rect)(const mde_rect_t *rect, void *ctx);
    uint32_t (*surface_create)(uint32_t w, uint32_t h, void *ctx);
    void (*surface_destroy)(uint32_t token, void *ctx);
    void (*surface_translate)(uint32_t token, int32_t dx, int32_t dy, void *ctx);
    void *ctx;
} mde_compositor_ops_t;

void wm_init(void);
void wm_set_compositor_ops(const mde_compositor_ops_t *ops);

int wm_window_create(const char *title, int32_t x, int32_t y,
                     int32_t w, int32_t h, uint32_t flags, uint32_t *out_id);
int wm_window_destroy(uint32_t id);
mde_window_t *wm_window_get(uint32_t id);

void wm_window_set_title(uint32_t id, const char *title);
mde_rect_t wm_window_client_rect(const mde_window_t *win);

uint32_t wm_window_count(void);
int wm_window_stack_bottom_to_top(uint32_t *ids, uint32_t cap, uint32_t *out_n);

int wm_window_raise(uint32_t id);
int wm_window_lower(uint32_t id);
uint32_t wm_window_topmost_id(void);

const mde_wm_interaction_t *wm_interaction_get(void);
int wm_move_begin(uint32_t id, int32_t px, int32_t py);
int wm_move_update(int32_t px, int32_t py);
int wm_move_end(void);

int wm_resize_begin(uint32_t id, uint32_t edges, int32_t px, int32_t py);
int wm_resize_update(int32_t px, int32_t py);
int wm_resize_end(void);

void wm_window_invalidate(uint32_t id);

/* Liquid Glass (LG-05): viewport, área útil y estados de ventana. */
void wm_set_viewport(int32_t width, int32_t height);
void wm_work_area_set(const mde_rect_t *area);
mde_rect_t wm_work_area_get(void);

int wm_window_minimize(uint32_t id);
int wm_window_maximize(uint32_t id);
int wm_window_restore(uint32_t id);
int wm_window_toggle_maximize(uint32_t id);

int wm_chrome_pointer_move(uint32_t id, int32_t px, int32_t py);
int wm_chrome_control_click(uint32_t id, int32_t px, int32_t py);

#endif /* MDE_WM_WINDOW_H */
/* MettaOS MDE — gestión de ventanas, Z-order y stubs move/resize (SA-19) */

#include "window.h"
#include "chrome.h"
#include "focus.h"

#include "../theme/controls.h"

#include <stddef.h>
#include <string.h>

#define MDE_WM_MIN_W  120
#define MDE_WM_MIN_H  80

static mde_window_t           g_windows[MDE_WM_MAX_WINDOWS];
static uint32_t               g_next_id       = 1u;
static uint32_t               g_z_counter     = 0u;
static mde_wm_interaction_t   g_interaction;
static mde_compositor_ops_t   g_comp;
static int                    g_wm_ready;
static int32_t                g_viewport_w;
static int32_t                g_viewport_h;
static mde_rect_t             g_work_area;
static uint8_t                g_work_area_custom;

static void wm_copy_title(char *dst, const char *src)
{
    size_t n = 0;

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    while (src[n] != '\0' && n < (size_t)MDE_WM_TITLE_MAX) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
}

static mde_window_t *wm_slot_find(uint32_t id)
{
    uint32_t i;

    for (i = 0; i < MDE_WM_MAX_WINDOWS; i++) {
        if (g_windows[i].in_use && g_windows[i].id == id) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static mde_window_t *wm_slot_alloc(void)
{
    uint32_t i;

    for (i = 0; i < MDE_WM_MAX_WINDOWS; i++) {
        if (!g_windows[i].in_use) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static void wm_invalidate_rect(const mde_rect_t *r)
{
    if (g_comp.invalidate_rect != NULL) {
        g_comp.invalidate_rect(r, g_comp.ctx);
    }
}

static void wm_invalidate_window(const mde_window_t *win)
{
    mde_rect_t frame;

    if (win == NULL) {
        return;
    }
    frame = wm_chrome_frame_rect(win);
    wm_invalidate_rect(&frame);
}

static uint32_t wm_surface_create_for(const mde_window_t *win)
{
    mde_rect_t body;

    if (g_comp.surface_create == NULL || win == NULL) {
        return 0u;
    }

    body = win->bounds;
    if ((win->flags & MDE_WIN_FLAG_DECORATED) != 0u) {
        body.h += (int32_t)wm_chrome_layer()->style.title_height;
    }
    if (body.w <= 0 || body.h <= 0) {
        return 0u;
    }
    return g_comp.surface_create((uint32_t)body.w, (uint32_t)body.h, g_comp.ctx);
}

static void wm_surface_destroy_token(uint32_t token)
{
    if (token != 0u && g_comp.surface_destroy != NULL) {
        g_comp.surface_destroy(token, g_comp.ctx);
    }
}

static int wm_rect_contains(const mde_rect_t *r, int32_t px, int32_t py)
{
    return px >= r->x && py >= r->y &&
           px < (r->x + r->w) && py < (r->y + r->h);
}

static void wm_interaction_reset(void)
{
    memset(&g_interaction, 0, sizeof(g_interaction));
}

static void wm_work_area_recompute(void)
{
    const uint32_t topbar_h    = MDE_CTRL_GLASS_TOPBAR_HEIGHT;
    const uint32_t dock_pad    = MDE_CTRL_DOCK_PADDING;
    const uint32_t dock_margin = MDE_CTRL_DOCK_MARGIN_BOTTOM;
    const uint32_t dock_h      = topbar_h + dock_pad * 2u;
    int32_t h                  = g_viewport_h;
    int32_t w                  = g_viewport_w;
    int32_t work_y             = (int32_t)topbar_h;
    int32_t work_h             = h - work_y;
    int32_t dock_y;
    int32_t bottom_inset;

    if (h <= 0 || w <= 0) {
        g_work_area.x = g_work_area.y = g_work_area.w = g_work_area.h = 0;
        return;
    }

    dock_y = (h > (int32_t)dock_h + (int32_t)dock_margin) ?
             h - (int32_t)dock_h - (int32_t)dock_margin : 0;
    bottom_inset = (h > dock_y) ? (h - dock_y) + (int32_t)dock_margin :
                   (int32_t)dock_margin;
    if (work_h > bottom_inset) {
        work_h -= bottom_inset;
    } else {
        work_h = 0;
    }

    g_work_area.x = 0;
    g_work_area.y = work_y;
    g_work_area.w = w;
    g_work_area.h = work_h;
}

static void wm_apply_maximized_bounds(mde_window_t *win)
{
    mde_rect_t wa;
    int32_t th;

    if (win == NULL || win->state != MDE_WIN_STATE_MAXIMIZED) {
        return;
    }

    wa = wm_work_area_get();
    th = (int32_t)wm_chrome_layer()->style.title_height;
    win->bounds.x = wa.x;
    win->bounds.y = wa.y;
    win->bounds.w = wa.w;
    win->bounds.h = wa.h - th;
    if ((win->flags & MDE_WIN_FLAG_DECORATED) == 0u) {
        win->bounds.h = wa.h;
    }
    if (win->bounds.h < MDE_WM_MIN_H) {
        win->bounds.h = MDE_WM_MIN_H;
    }
    if (win->bounds.w < MDE_WM_MIN_W) {
        win->bounds.w = MDE_WM_MIN_W;
    }
}

static void wm_refresh_maximized_windows(void)
{
    uint32_t i;

    for (i = 0; i < MDE_WM_MAX_WINDOWS; i++) {
        if (g_windows[i].in_use && g_windows[i].state == MDE_WIN_STATE_MAXIMIZED) {
            wm_apply_maximized_bounds(&g_windows[i]);
            wm_chrome_mark_dirty(&g_windows[i]);
            wm_invalidate_window(&g_windows[i]);
        }
    }
}

void wm_set_viewport(int32_t width, int32_t height)
{
    g_viewport_w = width;
    g_viewport_h = height;
    if (g_work_area_custom == 0u) {
        wm_work_area_recompute();
        wm_refresh_maximized_windows();
    }
}

void wm_work_area_set(const mde_rect_t *area)
{
    if (area == NULL) {
        g_work_area_custom = 0u;
        wm_work_area_recompute();
        wm_refresh_maximized_windows();
        return;
    }
    g_work_area        = *area;
    g_work_area_custom = 1u;
    wm_refresh_maximized_windows();
}

mde_rect_t wm_work_area_get(void)
{
    if (g_work_area_custom == 0u && g_viewport_w > 0 && g_viewport_h > 0) {
        wm_work_area_recompute();
    }
    return g_work_area;
}

void wm_init(void)
{
    uint32_t i;

    memset(g_windows, 0, sizeof(g_windows));
    memset(&g_comp, 0, sizeof(g_comp));
    wm_interaction_reset();
    g_next_id   = 1u;
    g_z_counter = 0u;
    g_wm_ready  = 1;

    for (i = 0; i < MDE_WM_MAX_WINDOWS; i++) {
        g_windows[i].id = 0u;
    }

    wm_chrome_init();
    wm_focus_init();
}

void wm_set_compositor_ops(const mde_compositor_ops_t *ops)
{
    if (ops == NULL) {
        memset(&g_comp, 0, sizeof(g_comp));
        return;
    }
    g_comp = *ops;
}

int wm_window_create(const char *title, int32_t x, int32_t y,
                     int32_t w, int32_t h, uint32_t flags, uint32_t *out_id)
{
    mde_window_t *win;

    if (!g_wm_ready || out_id == NULL) {
        return -1;
    }
    if (w < MDE_WM_MIN_W) {
        w = MDE_WM_MIN_W;
    }
    if (h < MDE_WM_MIN_H) {
        h = MDE_WM_MIN_H;
    }

    win = wm_slot_alloc();
    if (win == NULL) {
        return -2;
    }

    memset(win, 0, sizeof(*win));
    win->in_use   = 1u;
    win->id       = g_next_id++;
    win->flags    = flags;
    win->state    = MDE_WIN_STATE_NORMAL;
    win->z_order  = ++g_z_counter;
    win->bounds.x = x;
    win->bounds.y = y;
    win->bounds.w = w;
    win->bounds.h = h;
    win->restore_bounds = win->bounds;

    if ((flags & MDE_WIN_FLAG_DECORATED) == 0u) {
        win->flags |= MDE_WIN_FLAG_DECORATED;
    }

    wm_copy_title(win->title, title);
    win->surface_token = wm_surface_create_for(win);
    wm_chrome_mark_dirty(win);

    *out_id = win->id;
    wm_focus_set(win->id);
    wm_window_raise(win->id);
    wm_invalidate_window(win);
    return 0;
}

int wm_window_destroy(uint32_t id)
{
    mde_window_t *win = wm_slot_find(id);

    if (win == NULL) {
        return -1;
    }

    wm_invalidate_window(win);

    if (g_interaction.active && g_interaction.window_id == id) {
        wm_interaction_reset();
    }

    if (wm_focus_get() == id) {
        wm_focus_clear();
    }

    wm_surface_destroy_token(win->surface_token);
    memset(win, 0, sizeof(*win));
    return 0;
}

mde_window_t *wm_window_get(uint32_t id)
{
    return wm_slot_find(id);
}

void wm_window_invalidate(uint32_t id)
{
    wm_invalidate_window(wm_slot_find(id));
}

void wm_window_set_title(uint32_t id, const char *title)
{
    mde_window_t *win = wm_slot_find(id);

    if (win == NULL) {
        return;
    }
    wm_copy_title(win->title, title);
    wm_chrome_mark_dirty(win);
    wm_invalidate_window(win);
}

mde_rect_t wm_window_client_rect(const mde_window_t *win)
{
    if (win == NULL) {
        mde_rect_t z = {0, 0, 0, 0};
        return z;
    }
    return wm_chrome_client_rect(win);
}

uint32_t wm_window_count(void)
{
    uint32_t i;
    uint32_t n = 0u;

    for (i = 0; i < MDE_WM_MAX_WINDOWS; i++) {
        if (g_windows[i].in_use) {
            n++;
        }
    }
    return n;
}

static int wm_cmp_z(const mde_window_t *a, const mde_window_t *b)
{
    if (a->z_order < b->z_order) {
        return -1;
    }
    if (a->z_order > b->z_order) {
        return 1;
    }
    if (a->id < b->id) {
        return -1;
    }
    if (a->id > b->id) {
        return 1;
    }
    return 0;
}

int wm_window_stack_bottom_to_top(uint32_t *ids, uint32_t cap, uint32_t *out_n)
{
    mde_window_t *order[MDE_WM_MAX_WINDOWS];
    uint32_t i;
    uint32_t n = 0u;
    uint32_t k;

    if (ids == NULL || out_n == NULL || cap == 0u) {
        return -1;
    }

    for (i = 0; i < MDE_WM_MAX_WINDOWS; i++) {
        if (g_windows[i].in_use) {
            order[n++] = &g_windows[i];
        }
    }

    for (i = 0; i + 1u < n; i++) {
        for (k = i + 1u; k < n; k++) {
            if (wm_cmp_z(order[i], order[k]) > 0) {
                mde_window_t *tmp = order[i];
                order[i] = order[k];
                order[k] = tmp;
            }
        }
    }

    for (i = 0; i < n && i < cap; i++) {
        ids[i] = order[i]->id;
    }
    *out_n = (n < cap) ? n : cap;
    return (int)n;
}

int wm_window_raise(uint32_t id)
{
    mde_window_t *win = wm_slot_find(id);

    if (win == NULL) {
        return -1;
    }
    win->z_order = ++g_z_counter;
    wm_invalidate_window(win);
    return 0;
}

int wm_window_lower(uint32_t id)
{
    mde_window_t *win = wm_slot_find(id);
    mde_window_t *below = NULL;
    uint32_t i;
    uint32_t best = 0u;

    if (win == NULL) {
        return -1;
    }

    for (i = 0; i < MDE_WM_MAX_WINDOWS; i++) {
        if (!g_windows[i].in_use || g_windows[i].id == id) {
            continue;
        }
        if (g_windows[i].z_order < win->z_order &&
            (below == NULL || g_windows[i].z_order > best)) {
            below = &g_windows[i];
            best  = g_windows[i].z_order;
        }
    }

    if (below != NULL) {
        uint32_t tmp = win->z_order;
        win->z_order = below->z_order;
        below->z_order = tmp;
    } else {
        win->z_order = 0u;
    }

    wm_invalidate_window(win);
    if (below != NULL) {
        wm_invalidate_window(below);
    }
    return 0;
}

uint32_t wm_window_topmost_id(void)
{
    uint32_t i;
    uint32_t top_id = 0u;
    uint32_t top_z  = 0u;

    for (i = 0; i < MDE_WM_MAX_WINDOWS; i++) {
        if (!g_windows[i].in_use) {
            continue;
        }
        if (top_id == 0u || g_windows[i].z_order >= top_z) {
            top_z  = g_windows[i].z_order;
            top_id = g_windows[i].id;
        }
    }
    return top_id;
}

const mde_wm_interaction_t *wm_interaction_get(void)
{
    return &g_interaction;
}

int wm_move_begin(uint32_t id, int32_t px, int32_t py)
{
    mde_window_t *win = wm_slot_find(id);
    mde_rect_t titlebar;

    if (win == NULL || win->state == MDE_WIN_STATE_MAXIMIZED) {
        return -1;
    }

    titlebar = wm_chrome_titlebar_rect(win);
    if (!wm_rect_contains(&titlebar, px, py)) {
        return -2;
    }
    if (wm_chrome_control_hit(win, px, py) != MDE_WM_CTRL_NONE) {
        return -3;
    }

    wm_interaction_reset();
    g_interaction.active    = 1u;
    g_interaction.window_id = id;
    g_interaction.kind      = MDE_WM_DRAG_MOVE;
    g_interaction.pointer_x      = px;
    g_interaction.pointer_y      = py;
    g_interaction.last_pointer_x = px;
    g_interaction.last_pointer_y = py;
    g_interaction.start_bounds   = win->bounds;
    wm_focus_on_click(id);
    wm_window_raise(id);
    return 0;
}

int wm_move_update(int32_t px, int32_t py)
{
    mde_window_t *win;
    mde_rect_t old_frame;
    int32_t dx;
    int32_t dy;

    if (!g_interaction.active || g_interaction.kind != MDE_WM_DRAG_MOVE) {
        return -1;
    }

    win = wm_slot_find(g_interaction.window_id);
    if (win == NULL) {
        wm_interaction_reset();
        return -2;
    }

    old_frame = wm_chrome_frame_rect(win);
    dx = px - g_interaction.pointer_x;
    dy = py - g_interaction.pointer_y;

    win->bounds.x = g_interaction.start_bounds.x + dx;
    win->bounds.y = g_interaction.start_bounds.y + dy;

    if (win->surface_token != 0u && g_comp.surface_translate != NULL) {
        int32_t idx = px - g_interaction.last_pointer_x;
        int32_t idy = py - g_interaction.last_pointer_y;

        g_comp.surface_translate(win->surface_token, idx, idy, g_comp.ctx);
        g_interaction.last_pointer_x = px;
        g_interaction.last_pointer_y = py;
    }

    wm_invalidate_rect(&old_frame);
    wm_invalidate_window(win);
    wm_chrome_mark_dirty(win);
    return 0;
}

int wm_move_end(void)
{
    if (!g_interaction.active || g_interaction.kind != MDE_WM_DRAG_MOVE) {
        return -1;
    }
    wm_interaction_reset();
    return 0;
}

static void wm_apply_resize(mde_window_t *win, mde_rect_t *b,
                            int32_t px, int32_t py, uint32_t edges,
                            const mde_rect_t *start)
{
    int32_t right  = start->x + start->w;
    int32_t bottom = start->y + start->h;

    *b = *start;

    if (edges & MDE_WM_RESIZE_E) {
        b->w = px - start->x;
    }
    if (edges & MDE_WM_RESIZE_S) {
        b->h = py - start->y;
    }
    if (edges & MDE_WM_RESIZE_W) {
        b->x = px;
        b->w = right - px;
    }
    if (edges & MDE_WM_RESIZE_N) {
        b->y = py;
        b->h = bottom - py;
    }

    if (b->w < MDE_WM_MIN_W) {
        b->w = MDE_WM_MIN_W;
    }
    if (b->h < MDE_WM_MIN_H) {
        b->h = MDE_WM_MIN_H;
    }
}

int wm_resize_begin(uint32_t id, uint32_t edges, int32_t px, int32_t py)
{
    mde_window_t *win = wm_slot_find(id);

    if (win == NULL || (win->flags & MDE_WIN_FLAG_RESIZABLE) == 0u) {
        return -1;
    }
    if (win->state == MDE_WIN_STATE_MAXIMIZED) {
        return -2;
    }
    if (edges == 0u) {
        return -3;
    }

    wm_interaction_reset();
    g_interaction.active        = 1u;
    g_interaction.window_id     = id;
    g_interaction.kind          = MDE_WM_DRAG_RESIZE;
    g_interaction.resize_edges  = edges;
    g_interaction.pointer_x     = px;
    g_interaction.pointer_y     = py;
    g_interaction.start_bounds  = win->bounds;
    wm_focus_on_click(id);
    wm_window_raise(id);
    return 0;
}

int wm_resize_update(int32_t px, int32_t py)
{
    mde_window_t *win;
    mde_rect_t new_bounds;
    mde_rect_t old_frame;

    if (!g_interaction.active || g_interaction.kind != MDE_WM_DRAG_RESIZE) {
        return -1;
    }

    win = wm_slot_find(g_interaction.window_id);
    if (win == NULL) {
        wm_interaction_reset();
        return -2;
    }

    old_frame = wm_chrome_frame_rect(win);
    wm_apply_resize(win, &new_bounds, px, py,
                    g_interaction.resize_edges, &g_interaction.start_bounds);
    win->bounds = new_bounds;

    wm_invalidate_rect(&old_frame);
    wm_invalidate_window(win);
    wm_chrome_mark_dirty(win);
    return 0;
}

int wm_resize_end(void)
{
    mde_window_t *win;

    if (!g_interaction.active || g_interaction.kind != MDE_WM_DRAG_RESIZE) {
        return -1;
    }

    win = wm_slot_find(g_interaction.window_id);
    if (win != NULL) {
        /*
         * Stub: cambio de tamaño exige realloc de buffer en compositor;
         * hasta SA-15 solo se invalida. surface_token se conserva si el
         * tamaño no cambió materialmente.
         */
        wm_chrome_mark_dirty(win);
        wm_invalidate_window(win);
    }

    wm_interaction_reset();
    return 0;
}

int wm_window_minimize(uint32_t id)
{
    mde_window_t *win = wm_slot_find(id);

    if (win == NULL) {
        return -1;
    }
    if (win->state == MDE_WIN_STATE_MINIMIZED) {
        return 0;
    }
    win->state = MDE_WIN_STATE_MINIMIZED;
    wm_chrome_mark_dirty(win);
    wm_invalidate_window(win);
    return 0;
}

int wm_window_maximize(uint32_t id)
{
    mde_window_t *win = wm_slot_find(id);

    if (win == NULL) {
        return -1;
    }
    if (win->state == MDE_WIN_STATE_MAXIMIZED) {
        return 0;
    }
    if (g_interaction.active && g_interaction.window_id == id) {
        wm_interaction_reset();
    }
    win->restore_bounds = win->bounds;
    win->state          = MDE_WIN_STATE_MAXIMIZED;
    wm_apply_maximized_bounds(win);
    wm_focus_on_click(id);
    wm_chrome_mark_dirty(win);
    wm_invalidate_window(win);
    return 0;
}

int wm_window_restore(uint32_t id)
{
    mde_window_t *win = wm_slot_find(id);

    if (win == NULL) {
        return -1;
    }
    if (win->state == MDE_WIN_STATE_NORMAL) {
        return 0;
    }
    win->state  = MDE_WIN_STATE_NORMAL;
    win->bounds = win->restore_bounds;
    wm_chrome_mark_dirty(win);
    wm_invalidate_window(win);
    return 0;
}

int wm_window_toggle_maximize(uint32_t id)
{
    mde_window_t *win = wm_slot_find(id);

    if (win == NULL) {
        return -1;
    }
    if (win->state == MDE_WIN_STATE_MAXIMIZED) {
        return wm_window_restore(id);
    }
    return wm_window_maximize(id);
}

int wm_chrome_pointer_move(uint32_t id, int32_t px, int32_t py)
{
    mde_window_t *win = wm_slot_find(id);
    int changed;

    if (win == NULL) {
        return -1;
    }
    changed = wm_chrome_pointer_hover(win, px, py);
    if (changed != 0) {
        wm_invalidate_window(win);
    }
    return changed;
}

int wm_chrome_control_click(uint32_t id, int32_t px, int32_t py)
{
    mde_window_t *win = wm_slot_find(id);
    mde_wm_ctrl_id_t ctrl;

    if (win == NULL) {
        return -1;
    }

    ctrl = wm_chrome_control_hit(win, px, py);
    if (ctrl == MDE_WM_CTRL_NONE) {
        return -2;
    }

    wm_focus_on_click(id);

    switch (ctrl) {
    case MDE_WM_CTRL_CLOSE:
        return wm_window_destroy(id);
    case MDE_WM_CTRL_MINIMIZE:
        return wm_window_minimize(id);
    case MDE_WM_CTRL_MAXIMIZE:
        return wm_window_toggle_maximize(id);
    default:
        return -3;
    }
}

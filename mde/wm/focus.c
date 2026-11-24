/* MettaOS MDE — foco de ventana (SA-19) */

#include "focus.h"
#include "window.h"

static uint32_t g_focus_id;
static int      g_focus_ready;

void wm_focus_init(void)
{
    g_focus_id     = 0u;
    g_focus_ready  = 1;
}

uint32_t wm_focus_get(void)
{
    if (!g_focus_ready) {
        return 0u;
    }
    return g_focus_id;
}

static void wm_focus_invalidate_pair(uint32_t prev, uint32_t next)
{
    if (prev != 0u) {
        wm_window_invalidate(prev);
    }
    if (next != 0u && next != prev) {
        wm_window_invalidate(next);
    }
}

int wm_focus_set(uint32_t id)
{
    mde_window_t *win;
    uint32_t prev;

    if (!g_focus_ready) {
        return -1;
    }
    if (id == 0u) {
        return wm_focus_clear();
    }

    win = wm_window_get(id);
    if (win == NULL) {
        return -2;
    }

    prev        = g_focus_id;
    g_focus_id  = id;
    wm_focus_invalidate_pair(prev, g_focus_id);
    wm_window_raise(id);
    return 0;
}

int wm_focus_clear(void)
{
    uint32_t prev;

    if (!g_focus_ready) {
        return -1;
    }

    prev       = g_focus_id;
    g_focus_id = 0u;
    wm_focus_invalidate_pair(prev, 0u);
    return 0;
}

static int wm_focus_cycle(int dir)
{
    uint32_t stack[MDE_WM_MAX_WINDOWS];
    uint32_t n = 0u;
    uint32_t i;
    int found = -1;
    uint32_t pick = 0u;

    if (wm_window_stack_bottom_to_top(stack, MDE_WM_MAX_WINDOWS, &n) < 0 || n == 0u) {
        return -1;
    }

    for (i = 0; i < n; i++) {
        if (stack[i] == g_focus_id) {
            found = (int)i;
            break;
        }
    }

    if (dir > 0) {
        /* Siguiente: subir en Z (índice mayor). */
        if (found >= 0 && (uint32_t)(found + 1) < n) {
            pick = stack[found + 1u];
        } else if (n > 0u) {
            pick = stack[0];
        }
    } else {
        /* Anterior: bajar en Z. */
        if (found > 0) {
            pick = stack[(uint32_t)found - 1u];
        } else if (n > 0u) {
            pick = stack[n - 1u];
        }
    }

    if (pick == 0u) {
        return -2;
    }
    return wm_focus_set(pick);
}

int wm_focus_next(void)
{
    return wm_focus_cycle(1);
}

int wm_focus_prev(void)
{
    return wm_focus_cycle(-1);
}

int wm_focus_on_click(uint32_t id)
{
    wm_window_raise(id);
    return wm_focus_set(id);
}
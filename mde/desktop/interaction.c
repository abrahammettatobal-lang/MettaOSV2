/*
 * MDE desktop — hover, selección rubber-band glass (SA-22 / LG-06).
 */

#include <stddef.h>
#include <stdint.h>

#include "../theme/glass.h"

void mde_anim_init(void);
uint32_t mde_anim_tick(uint64_t now_ms);
int mde_anim_start(uint32_t channel_id, int32_t from, int32_t to,
                   uint32_t duration_ms, uint8_t ease_id, uint64_t now_ms);
int32_t mde_anim_value(uint32_t channel_id);

#define MDE_ICON_HOVER_CHANNEL_BASE  0u
/* Escala en milésimas: 1000 = 1.0, 1080 = 1.08 */
#define MDE_ICON_HOVER_SCALE_REST      1000
#define MDE_ICON_HOVER_SCALE_ACTIVE    1080
#define MDE_ICON_HOVER_ANIM_MS         120u

#define MDE_HOVER_SWITCH_DELAY_MS      48u
#define MDE_HOVER_LEAVE_GRACE_MS       90u

#define MDE_DESKTOP_MAX_ICONS          128u
/* Canales 60–63 reservados para menú/launcher (animations.c). */
#define MDE_ICON_HOVER_MAX_ID          59
#define MDE_SEL_FILL                   MDE_COLOR_RGBA(0x99u, 0xD5E6FFu)
#define MDE_SEL_BORDER                 MDE_COLOR_RGBA(0xCCu, 0x3278D9u)

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t id;
} mde_desktop_icon_t;

typedef struct {
    int32_t x;
    int32_t y;
} mde_pointer_t;

static const mde_desktop_icon_t *g_icons;
static uint32_t                    g_icon_count;

static mde_pointer_t g_ptr;
static int32_t       g_hover_id;
static int32_t       g_pending_id;
static uint64_t      g_pending_since_ms;
static uint64_t      g_leave_grace_until_ms;

static int     g_sel_active;
static int32_t g_sel_ax;
static int32_t g_sel_ay;
static int32_t g_sel_bx;
static int32_t g_sel_by;

static int mde_point_in_rect(int32_t px, int32_t py, const mde_desktop_icon_t *ic)
{
    return px >= ic->x && py >= ic->y &&
           px < (ic->x + ic->w) && py < (ic->y + ic->h);
}

static int32_t mde_icon_hit_test(int32_t px, int32_t py)
{
    if (g_icons == NULL || g_icon_count == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < g_icon_count; i++) {
        if (mde_point_in_rect(px, py, &g_icons[i])) {
            return g_icons[i].id;
        }
    }
    return -1;
}

static uint32_t mde_hover_channel_for_id(int32_t icon_id)
{
    if (icon_id < 0) {
        return MDE_ICON_HOVER_CHANNEL_BASE;
    }
    if (icon_id > MDE_ICON_HOVER_MAX_ID) {
        icon_id = MDE_ICON_HOVER_MAX_ID;
    }
    return MDE_ICON_HOVER_CHANNEL_BASE + (uint32_t)icon_id;
}

static void mde_apply_hover_visual(int32_t new_id, int32_t old_id, uint64_t now_ms)
{
    if (old_id >= 0 && old_id != new_id) {
        mde_anim_start(mde_hover_channel_for_id(old_id),
                       mde_anim_value(mde_hover_channel_for_id(old_id)),
                       MDE_ICON_HOVER_SCALE_REST,
                       MDE_ICON_HOVER_ANIM_MS, 1, now_ms);
    }

    if (new_id >= 0) {
        mde_anim_start(mde_hover_channel_for_id(new_id),
                       mde_anim_value(mde_hover_channel_for_id(new_id)),
                       MDE_ICON_HOVER_SCALE_ACTIVE,
                       MDE_ICON_HOVER_ANIM_MS, 1, now_ms);
    }
}

static void mde_sel_normalize(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    int32_t x0 = g_sel_ax;
    int32_t y0 = g_sel_ay;
    int32_t x1 = g_sel_bx;
    int32_t y1 = g_sel_by;

    if (x1 < x0) {
        int32_t t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y1 < y0) {
        int32_t t = y0;
        y0 = y1;
        y1 = t;
    }

    if (x != NULL) {
        *x = x0;
    }
    if (y != NULL) {
        *y = y0;
    }
    if (w != NULL) {
        *w = x1 - x0;
    }
    if (h != NULL) {
        *h = y1 - y0;
    }
}

void mde_interaction_init(void)
{
    g_icons                = NULL;
    g_icon_count           = 0;
    g_ptr.x                = 0;
    g_ptr.y                = 0;
    g_hover_id             = -1;
    g_pending_id           = -1;
    g_pending_since_ms     = 0;
    g_leave_grace_until_ms = 0;
    g_sel_active           = 0;
    g_sel_ax               = 0;
    g_sel_ay               = 0;
    g_sel_bx               = 0;
    g_sel_by               = 0;

    mde_anim_init();
}

void mde_interaction_set_icons(const mde_desktop_icon_t *icons, uint32_t count)
{
    if (icons == NULL || count == 0) {
        g_icons      = NULL;
        g_icon_count = 0;
        g_hover_id   = -1;
        g_pending_id = -1;
        return;
    }

    if (count > MDE_DESKTOP_MAX_ICONS) {
        count = MDE_DESKTOP_MAX_ICONS;
    }

    g_icons      = icons;
    g_icon_count = count;
}

void mde_interaction_pointer_move(int32_t x, int32_t y, uint64_t now_ms)
{
    g_ptr.x = x;
    g_ptr.y = y;

    if (g_sel_active) {
        g_sel_bx = x;
        g_sel_by = y;
    }

    int32_t hit = mde_icon_hit_test(x, y);

    if (hit < 0) {
        if (g_hover_id >= 0) {
            if (g_leave_grace_until_ms == 0) {
                g_leave_grace_until_ms = now_ms + MDE_HOVER_LEAVE_GRACE_MS;
            }
            if (now_ms >= g_leave_grace_until_ms) {
                int32_t old = g_hover_id;
                g_hover_id             = -1;
                g_pending_id           = -1;
                g_leave_grace_until_ms = 0;
                mde_apply_hover_visual(-1, old, now_ms);
            }
        } else {
            g_leave_grace_until_ms = 0;
            g_pending_id           = -1;
        }
        return;
    }

    g_leave_grace_until_ms = 0;

    if (hit == g_hover_id) {
        g_pending_id       = -1;
        g_pending_since_ms = 0;
        return;
    }

    if (g_hover_id < 0) {
        g_hover_id = hit;
        mde_apply_hover_visual(hit, -1, now_ms);
        g_pending_id = -1;
        return;
    }

    if (hit == g_pending_id) {
        if (now_ms - g_pending_since_ms >= MDE_HOVER_SWITCH_DELAY_MS) {
            int32_t old = g_hover_id;
            g_hover_id         = hit;
            g_pending_id       = -1;
            g_pending_since_ms = 0;
            mde_apply_hover_visual(hit, old, now_ms);
        }
    } else {
        g_pending_id       = hit;
        g_pending_since_ms = now_ms;
    }
}

void mde_interaction_selection_begin(int32_t x, int32_t y)
{
    g_sel_active = 1;
    g_sel_ax     = x;
    g_sel_ay     = y;
    g_sel_bx     = x;
    g_sel_by     = y;
}

void mde_interaction_selection_update(int32_t x, int32_t y)
{
    if (!g_sel_active) {
        return;
    }
    g_sel_bx = x;
    g_sel_by = y;
}

void mde_interaction_selection_end(void)
{
    g_sel_active = 0;
}

int mde_interaction_selection_active(void)
{
    return g_sel_active;
}

void mde_interaction_selection_bounds(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    if (!g_sel_active) {
        if (x != NULL) {
            *x = 0;
        }
        if (y != NULL) {
            *y = 0;
        }
        if (w != NULL) {
            *w = 0;
        }
        if (h != NULL) {
            *h = 0;
        }
        return;
    }
    mde_sel_normalize(x, y, w, h);
}

uint32_t mde_interaction_selection_fill(void)
{
    return MDE_SEL_FILL;
}

uint32_t mde_interaction_selection_border(void)
{
    return MDE_SEL_BORDER;
}

int32_t mde_interaction_hover_icon_id(void)
{
    return g_hover_id;
}

int32_t mde_interaction_icon_hover_scale(int32_t icon_id)
{
    if (icon_id < 0) {
        return MDE_ICON_HOVER_SCALE_REST;
    }
    return mde_anim_value(mde_hover_channel_for_id(icon_id));
}

void mde_interaction_tick(uint64_t now_ms)
{
    (void)mde_anim_tick(now_ms);

    if (g_hover_id >= 0 && g_leave_grace_until_ms != 0 &&
        now_ms >= g_leave_grace_until_ms) {
        int32_t old = g_hover_id;
        g_hover_id             = -1;
        g_leave_grace_until_ms = 0;
        g_pending_id           = -1;
        mde_apply_hover_visual(-1, old, now_ms);
    }
}
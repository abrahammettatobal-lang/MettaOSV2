/* MettaOS MDE — chrome ventana Liquid Glass Tahoe (LG-T05) */

#include "chrome.h"

#include <stddef.h>

/* Traffic lights macOS clásicos */
#define MDE_WM_CTRL_CLOSE_RGB        0xFF5F57u
#define MDE_WM_CTRL_CLOSE_HOVER_RGB  0xFF6F68u
#define MDE_WM_CTRL_MIN_RGB          0xFEBC2Eu
#define MDE_WM_CTRL_MIN_HOVER_RGB    0xFFCC4Du
#define MDE_WM_CTRL_ZOOM_RGB         0x28C840u
#define MDE_WM_CTRL_ZOOM_HOVER_RGB   0x3CDD53u
#define MDE_WM_CTRL_INACTIVE_RGB     0xD1D1D6u
#define MDE_WM_CTRL_INACTIVE_HOVER   0xB8B8BDu

static mde_chrome_layer_t g_chrome;
static int                g_chrome_ready;

static mde_rect_t wm_rect_union_pad(const mde_rect_t *a, int32_t pad)
{
    mde_rect_t u;

    u.x = a->x - pad;
    u.y = a->y - pad;
    u.w = a->w + pad * 2;
    u.h = a->h + pad * 2;
    return u;
}

static int wm_rect_contains(const mde_rect_t *r, int32_t px, int32_t py)
{
    return px >= r->x && py >= r->y &&
           px < (r->x + r->w) && py < (r->y + r->h);
}

static int wm_chrome_focused(const mde_window_t *win, uint32_t focused_id)
{
    return win != NULL && win->id == focused_id && focused_id != 0u;
}

static int wm_chrome_cmd_push(mde_wm_chrome_draw_list_t *list,
                              mde_wm_chrome_rect_cmd_t cmd)
{
    if (list == NULL || list->count >= MDE_WM_MAX_DRAW_CMDS) {
        return -1;
    }
    list->cmds[list->count++] = cmd;
    return 0;
}

static uint8_t wm_shadow_alpha_layer(uint8_t base, const mde_window_t *win,
                                     uint32_t focused_id)
{
    if (wm_chrome_focused(win, focused_id)) {
        return base;
    }
    return (uint8_t)((base * 55u) / 100u);
}

void wm_chrome_init(void)
{
    const mde_glass_tokens *g = &mde_light_glass;

    mde_window_glass_style_light(&g_chrome.style, &g_chrome.glass);
    g_chrome.tokens            = g;
    g_chrome.shadow_offset.x   = 0;
    g_chrome.shadow_offset.y   = 4;
    g_chrome.shadow_alpha      = g->shadow[1].alpha;
    g_chrome.client_fill       = MDE_COLOR_RGB(0xFFFFFFu);
    g_chrome.client_glass      = MDE_COLOR_RGBA(0xF2u, 0xFFFFFFu);
    g_chrome.client_use_glass  = 0u;
    g_chrome_ready             = 1;
}

const mde_chrome_layer_t *wm_chrome_layer(void)
{
    if (!g_chrome_ready) {
        wm_chrome_init();
    }
    return &g_chrome;
}

mde_rect_t wm_chrome_body_rect(const mde_window_t *win)
{
    mde_rect_t r;

    if (win == NULL) {
        r.x = r.y = r.w = r.h = 0;
        return r;
    }

    r = win->bounds;
    if ((win->flags & MDE_WIN_FLAG_DECORATED) != 0u) {
        r.h += (int32_t)g_chrome.style.title_height;
    }
    return r;
}

mde_rect_t wm_chrome_frame_rect(const mde_window_t *win)
{
    mde_rect_t body = wm_chrome_body_rect(win);

    return wm_rect_union_pad(&body, MDE_WM_SHADOW_PAD_GLASS);
}

mde_rect_t wm_chrome_titlebar_rect(const mde_window_t *win)
{
    mde_rect_t r;

    if (win == NULL || (win->flags & MDE_WIN_FLAG_DECORATED) == 0u) {
        r.x = r.y = r.w = r.h = 0;
        return r;
    }

    r.x = win->bounds.x;
    r.y = win->bounds.y;
    r.w = win->bounds.w;
    r.h = (int32_t)g_chrome.style.title_height;
    return r;
}

mde_rect_t wm_chrome_client_rect(const mde_window_t *win)
{
    mde_rect_t r;

    if (win == NULL) {
        r.x = r.y = r.w = r.h = 0;
        return r;
    }

    r = win->bounds;
    if ((win->flags & MDE_WIN_FLAG_DECORATED) != 0u) {
        r.y += (int32_t)g_chrome.style.title_height;
        r.h -= (int32_t)g_chrome.style.title_height;
        if (r.h < 0) {
            r.h = 0;
        }
    }
    return r;
}

mde_rect_t wm_chrome_title_label_rect(const mde_window_t *win)
{
    mde_rect_t tb;
    mde_rect_t r;
    int32_t strip;

    tb = wm_chrome_titlebar_rect(win);
    if (win == NULL || (win->flags & MDE_WIN_FLAG_DECORATED) == 0u || tb.w <= 0) {
        r.x = r.y = r.w = r.h = 0;
        return r;
    }

    strip = (int32_t)MDE_WM_CTRL_MARGIN_LEFT +
            (int32_t)(3u * (MDE_WM_CTRL_SIZE + MDE_WM_CTRL_GAP)) + 8;
    if (strip * 2 >= tb.w) {
        strip = tb.w / 4;
    }
    r.x = tb.x + strip;
    r.y = tb.y;
    r.w = tb.w - strip * 2;
    r.h = tb.h;
    return r;
}

uint32_t wm_chrome_titlebar_color(const mde_window_t *win, uint32_t focused_id)
{
    const mde_chrome_layer_t *layer = wm_chrome_layer();
    const mde_glass_tokens *g       = layer->tokens;

    if (win == NULL || g == NULL) {
        return layer->glass.fill;
    }
    if (wm_chrome_focused(win, focused_id)) {
        return g->fill_primary;
    }
    return g->fill_chrome;
}

uint32_t wm_chrome_client_color(const mde_window_t *win, uint32_t focused_id)
{
    const mde_chrome_layer_t *layer = wm_chrome_layer();

    (void)focused_id;
    if (win == NULL) {
        return layer->client_fill;
    }
    if (layer->client_use_glass != 0u) {
        return layer->client_glass;
    }
    return layer->client_fill;
}

uint32_t wm_chrome_title_text_color(const mde_window_t *win, uint32_t focused_id)
{
    const mde_chrome_layer_t *layer = wm_chrome_layer();

    if (win == NULL) {
        return layer->style.title_text;
    }
    if (wm_chrome_focused(win, focused_id)) {
        return layer->style.title_text;
    }
    return MDE_COLOR_RGB(0x8A95A8u);
}

uint8_t wm_chrome_titlebar_blur(const mde_window_t *win, uint32_t focused_id)
{
    const mde_chrome_layer_t *layer = wm_chrome_layer();
    const mde_glass_tokens *g       = layer->tokens;

    if (g == NULL) {
        return layer->glass.blur_radius;
    }
    if (wm_chrome_focused(win, focused_id)) {
        return layer->glass.blur_radius;
    }
    return g->blur_light;
}

uint8_t wm_chrome_frame_border_alpha(const mde_window_t *win, uint32_t focused_id)
{
    if (wm_chrome_focused(win, focused_id)) {
        return 0xFFu;
    }
    return 0x99u;
}

static mde_rect_t wm_chrome_ctrl_rect_at_left(const mde_window_t *win,
                                               uint32_t index_from_left)
{
    mde_rect_t tb = wm_chrome_titlebar_rect(win);
    mde_rect_t r;
    uint32_t sz = MDE_WM_CTRL_SIZE;
    uint32_t gap = MDE_WM_CTRL_GAP;
    int32_t x;

    x = tb.x + (int32_t)MDE_WM_CTRL_MARGIN_LEFT;
    x += (int32_t)(index_from_left * (sz + gap));
    r.x = x;
    r.y = tb.y + (tb.h - (int32_t)sz) / 2;
    r.w = (int32_t)sz;
    r.h = (int32_t)sz;
    return r;
}

mde_rect_t wm_chrome_control_rect(const mde_window_t *win, mde_wm_ctrl_id_t ctrl)
{
    mde_rect_t z = {0, 0, 0, 0};

    if (win == NULL || (win->flags & MDE_WIN_FLAG_DECORATED) == 0u) {
        return z;
    }
    switch (ctrl) {
    case MDE_WM_CTRL_CLOSE:
        return wm_chrome_ctrl_rect_at_left(win, 0u);
    case MDE_WM_CTRL_MINIMIZE:
        return wm_chrome_ctrl_rect_at_left(win, 1u);
    case MDE_WM_CTRL_MAXIMIZE:
        return wm_chrome_ctrl_rect_at_left(win, 2u);
    default:
        return z;
    }
}

uint32_t wm_chrome_control_color(mde_wm_ctrl_id_t ctrl, uint8_t hover,
                                 const mde_window_t *win, uint32_t focused_id)
{
    int focused = wm_chrome_focused(win, focused_id);

    if (!focused) {
        if (hover != 0u) {
            return MDE_COLOR_RGB(MDE_WM_CTRL_INACTIVE_HOVER);
        }
        return MDE_COLOR_RGB(MDE_WM_CTRL_INACTIVE_RGB);
    }

    switch (ctrl) {
    case MDE_WM_CTRL_CLOSE:
        return hover ? MDE_COLOR_RGB(MDE_WM_CTRL_CLOSE_HOVER_RGB)
                     : MDE_COLOR_RGB(MDE_WM_CTRL_CLOSE_RGB);
    case MDE_WM_CTRL_MINIMIZE:
        return hover ? MDE_COLOR_RGB(MDE_WM_CTRL_MIN_HOVER_RGB)
                     : MDE_COLOR_RGB(MDE_WM_CTRL_MIN_RGB);
    case MDE_WM_CTRL_MAXIMIZE:
        return hover ? MDE_COLOR_RGB(MDE_WM_CTRL_ZOOM_HOVER_RGB)
                     : MDE_COLOR_RGB(MDE_WM_CTRL_ZOOM_RGB);
    default:
        return MDE_COLOR_RGB(MDE_WM_CTRL_INACTIVE_RGB);
    }
}

mde_wm_ctrl_id_t wm_chrome_control_hit(const mde_window_t *win, int32_t px, int32_t py)
{
    mde_rect_t r;

    if (win == NULL || (win->flags & MDE_WIN_FLAG_DECORATED) == 0u) {
        return MDE_WM_CTRL_NONE;
    }
    r = wm_chrome_control_rect(win, MDE_WM_CTRL_CLOSE);
    if (wm_rect_contains(&r, px, py)) {
        return MDE_WM_CTRL_CLOSE;
    }
    r = wm_chrome_control_rect(win, MDE_WM_CTRL_MINIMIZE);
    if (wm_rect_contains(&r, px, py)) {
        return MDE_WM_CTRL_MINIMIZE;
    }
    r = wm_chrome_control_rect(win, MDE_WM_CTRL_MAXIMIZE);
    if (wm_rect_contains(&r, px, py)) {
        return MDE_WM_CTRL_MAXIMIZE;
    }
    return MDE_WM_CTRL_NONE;
}

void wm_chrome_mark_dirty(mde_window_t *win)
{
    if (win != NULL) {
        win->chrome_dirty = 1u;
    }
}

void wm_chrome_invalidate_window(const mde_window_t *win)
{
    (void)win;
}

uint32_t wm_chrome_collect_damage(const mde_window_t *win, uint32_t focused_id,
                                  mde_rect_t *out, uint32_t cap)
{
    const mde_glass_tokens *g;
    mde_rect_t body;
    uint32_t i;
    uint32_t n = 0u;

    if (win == NULL || out == NULL || cap == 0u) {
        return 0u;
    }

    g   = wm_chrome_layer()->tokens;
    body = wm_chrome_body_rect(win);

    if (n < cap) {
        out[n++] = wm_chrome_titlebar_rect(win);
        (void)focused_id;
    }
    if (n < cap) {
        out[n++] = wm_chrome_client_rect(win);
    }
    if (g != NULL) {
        for (i = 0u; i < MDE_GLASS_SHADOW_LAYERS && n < cap; i++) {
            mde_rect_t sh = body;
            int32_t pad = (int32_t)g->shadow[i].blur_radius;

            sh.x += (int32_t)g->shadow[i].offset_x;
            sh.y += (int32_t)g->shadow[i].offset_y;
            sh.w += pad;
            sh.h += pad;
            out[n++] = sh;
        }
    }
    return n;
}

static void wm_chrome_push_resize_grips(const mde_window_t *win, uint32_t focused_id,
                                        mde_wm_chrome_draw_list_t *out,
                                        uint32_t grip_color)
{
    mde_rect_t body;
    int32_t gs = (int32_t)MDE_WM_GRIP_SIZE;
    int32_t in = (int32_t)MDE_WM_GRIP_INSET;
    uint8_t ga;
    mde_wm_chrome_rect_cmd_t grip;

    if ((win->flags & MDE_WIN_FLAG_RESIZABLE) == 0u) {
        return;
    }

    body = wm_chrome_body_rect(win);
    ga   = wm_chrome_focused(win, focused_id) ? (uint8_t)0x44u : (uint8_t)0x28u;
    grip = (mde_wm_chrome_rect_cmd_t){
        .color         = grip_color,
        .alpha         = ga,
        .kind          = (uint8_t)MDE_WM_CHROME_CMD_GRIP,
        .blur_radius   = 0u,
        .corner_radius = 1u,
    };

    grip.rect.x = body.x + body.w - in - gs;
    grip.rect.y = body.y + body.h - in - gs;
    grip.rect.w = gs;
    grip.rect.h = gs;
    (void)wm_chrome_cmd_push(out, grip);

    grip.rect.x = body.x + in;
    grip.rect.y = body.y + body.h - in - gs;
    (void)wm_chrome_cmd_push(out, grip);

    grip.rect.x = body.x + body.w - in - gs;
    grip.rect.y = body.y + in;
    (void)wm_chrome_cmd_push(out, grip);

    grip.rect.x = body.x + in;
    grip.rect.y = body.y + in;
    (void)wm_chrome_cmd_push(out, grip);
}

int wm_chrome_build_draw_list(const mde_window_t *win, uint32_t focused_id,
                              mde_wm_chrome_draw_list_t *out)
{
    const mde_chrome_layer_t *layer;
    const mde_glass_tokens *g;
    mde_rect_t body;
    mde_rect_t client;
    mde_rect_t title;
    uint32_t i;
    uint16_t radius;
    uint8_t border_alpha;
    int focused;

    if (win == NULL || out == NULL ||
        win->state == MDE_WIN_STATE_MINIMIZED) {
        return -1;
    }

    out->count   = 0u;
    layer        = wm_chrome_layer();
    g            = layer->tokens;
    body         = wm_chrome_body_rect(win);
    title        = wm_chrome_titlebar_rect(win);
    client       = wm_chrome_client_rect(win);
    radius       = layer->style.corner_radius;
    border_alpha = wm_chrome_frame_border_alpha(win, focused_id);
    focused      = wm_chrome_focused(win, focused_id);

    if (g != NULL) {
        for (i = 0u; i < MDE_GLASS_SHADOW_LAYERS; i++) {
            mde_wm_chrome_rect_cmd_t shadow = {
                .rect           = body,
                .color          = MDE_COLOR_RGB(0x000000u),
                .alpha          = wm_shadow_alpha_layer(g->shadow[i].alpha, win, focused_id),
                .kind           = (uint8_t)MDE_WM_CHROME_CMD_SHADOW,
                .blur_radius    = g->shadow[i].blur_radius,
                .corner_radius  = radius,
            };
            shadow.rect.x += (int32_t)g->shadow[i].offset_x;
            shadow.rect.y += (int32_t)g->shadow[i].offset_y;
            if (wm_chrome_cmd_push(out, shadow) != 0) {
                break;
            }
        }
    }

    if ((win->flags & MDE_WIN_FLAG_DECORATED) != 0u) {
        uint8_t tb_alpha = focused ? (uint8_t)0xFFu : (uint8_t)0xE8u;
        mde_wm_chrome_rect_cmd_t tb = {
            .rect           = title,
            .color          = wm_chrome_titlebar_color(win, focused_id),
            .alpha          = tb_alpha,
            .kind           = (uint8_t)MDE_WM_CHROME_CMD_FILL,
            .blur_radius    = wm_chrome_titlebar_blur(win, focused_id),
            .corner_radius  = radius,
        };
        (void)wm_chrome_cmd_push(out, tb);

        if (g != NULL && g->specular_top_height > 0u) {
            mde_wm_chrome_rect_cmd_t spec = {
                .rect = title,
                .color = g->specular_top,
                .alpha = focused ? (uint8_t)0xFFu : (uint8_t)0x73u,
                .kind = (uint8_t)MDE_WM_CHROME_CMD_SPECULAR,
                .blur_radius = 0u,
                .corner_radius = radius,
            };
            spec.rect.h = (int32_t)g->specular_top_height;
            (void)wm_chrome_cmd_push(out, spec);
        }
    }

    {
        mde_wm_chrome_rect_cmd_t fill = {
            .rect          = client,
            .color         = wm_chrome_client_color(win, focused_id),
            .alpha         = 0xFFu,
            .kind          = (uint8_t)MDE_WM_CHROME_CMD_FILL,
            .blur_radius   = layer->client_use_glass ? layer->glass.blur_radius : 0u,
            .corner_radius = radius,
        };
        (void)wm_chrome_cmd_push(out, fill);
    }

    if (g != NULL) {
        mde_wm_chrome_rect_cmd_t border_out = {
            .rect          = body,
            .color         = g->border_base,
            .alpha         = border_alpha,
            .kind          = (uint8_t)MDE_WM_CHROME_CMD_BORDER,
            .blur_radius   = 0u,
            .corner_radius = radius,
        };
        mde_wm_chrome_rect_cmd_t border_spec = border_out;
        border_spec.color = g->border_specular;
        border_spec.alpha = focused ? (uint8_t)0xCCu : (uint8_t)0x66u;
        (void)wm_chrome_cmd_push(out, border_out);
        (void)wm_chrome_cmd_push(out, border_spec);
    }

    if ((win->flags & MDE_WIN_FLAG_DECORATED) != 0u) {
        static const mde_wm_ctrl_id_t order[3] = {
            MDE_WM_CTRL_CLOSE,
            MDE_WM_CTRL_MINIMIZE,
            MDE_WM_CTRL_MAXIMIZE,
        };
        for (i = 0u; i < 3u; i++) {
            mde_wm_ctrl_id_t cid = order[i];
            mde_rect_t cr        = wm_chrome_control_rect(win, cid);
            uint8_t hover        = (win->chrome_ctrl_hover & (uint8_t)cid) != 0u;
            mde_wm_chrome_rect_cmd_t cc = {
                .rect          = cr,
                .color         = wm_chrome_control_color(cid, hover, win, focused_id),
                .alpha         = 0xFFu,
                .kind          = (uint8_t)MDE_WM_CHROME_CMD_CTRL,
                .blur_radius   = (hover && focused) ? (uint8_t)MDE_GLASS_BLUR_LIGHT : 0u,
                .corner_radius = (uint16_t)(MDE_WM_CTRL_SIZE / 2u),
            };
            (void)wm_chrome_cmd_push(out, cc);
        }

        {
            mde_rect_t label = wm_chrome_title_label_rect(win);
            mde_wm_chrome_rect_cmd_t tc = {
                .rect          = label,
                .color         = wm_chrome_title_text_color(win, focused_id),
                .alpha         = focused ? (uint8_t)0xFFu : (uint8_t)0xB3u,
                .kind          = (uint8_t)MDE_WM_CHROME_CMD_TITLE,
                .blur_radius   = 0u,
                .corner_radius = 0u,
            };
            (void)wm_chrome_cmd_push(out, tc);
        }
    }

    wm_chrome_push_resize_grips(win, focused_id, out,
                                g != NULL ? g->border_base : MDE_COLOR_RGB(0x687385u));

    return (int)out->count;
}

int wm_chrome_pointer_hover(mde_window_t *win, int32_t px, int32_t py)
{
    mde_wm_ctrl_id_t hit;
    uint8_t next;
    uint8_t prev;

    if (win == NULL) {
        return -1;
    }

    hit  = wm_chrome_control_hit(win, px, py);
    next = (uint8_t)hit;
    prev = win->chrome_ctrl_hover;
    win->chrome_ctrl_hover = next;
    if (next != prev) {
        wm_chrome_mark_dirty(win);
        return 1;
    }
    return 0;
}
/*
 * Bucle de frame del compositor: delega escena, ventanas y entrada en el
 * shell MDE (mde/ui). El compositor sólo gestiona daño, cursor y present.
 */
#include "compositor.h"

#include "../ui/ui.h"

#include <input/input.h>
#include <input/ps2.h>
#include <linux/usermode.h>
#include <metta/boot_info.h>
#include <log/klog.h>
#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);

#define COMP_INPUT_BUDGET 128u
#define COMP_SCENE_MAX_PX (1024u * 768u)

/* launcher vive en mde/apps/ (no siempre enlazado): stubs débiles */
__attribute__((weak)) int mde_launcher_init(void)
{
    return 0;
}
__attribute__((weak)) int mde_launcher_spawn(const char *app_id)
{
    (void)app_id;
    return 0;
}

typedef struct wallpaper_image {
    const uint32_t *pixels;
    uint32_t        width;
    uint32_t        height;
    uint32_t        format;
} wallpaper_image_t;

__attribute__((weak)) const wallpaper_image_t *wallpaper_get_active(void)
{
    return NULL;
}

static uint32_t g_scene[COMP_SCENE_MAX_PX];
static int      g_ui_ready;
static compositor_t *g_comp;

void ui_invalidate(int32_t x, int32_t y, int32_t w, int32_t h);

static void comp_scene_copy(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h,
                            uint32_t *backbuf, int32_t stride_px)
{
    compositor_t *c = (compositor_t *)ctx;
    int32_t row;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > (int32_t)c->width)
        w = (int32_t)c->width - x;
    if (y + h > (int32_t)c->height)
        h = (int32_t)c->height - y;
    if (w <= 0 || h <= 0)
        return;

    for (row = 0; row < h; row++) {
        memcpy(backbuf + (int64_t)(y + row) * stride_px + x,
               g_scene + (int64_t)(y + row) * (int64_t)c->width + x,
               (size_t)w * 4u);
    }
}

int compositor_shell_bootstrap(compositor_t *c)
{
    if (!c || !c->ready)
        return -1;
    g_comp = c;
    if ((uint64_t)c->width * c->height > COMP_SCENE_MAX_PX) {
        KLOG_ERR("shell: resolución %ux%u excede la escena\n", c->width, c->height);
        return -1;
    }

    if (input_init() != 0) {
        KLOG_WARN("input_init failed; shell sin ratón\n");
    } else {
        ps2_set_screen_bounds((int32_t)c->width, (int32_t)c->height);
        ps2_set_position((int32_t)c->width / 2, (int32_t)c->height / 2);
    }

    c->cfg.cursor_w = UI_CURSOR_W;
    c->cfg.cursor_h = UI_CURSOR_H;
    c->pointer_x = (int32_t)c->width / 2;
    c->pointer_y = (int32_t)c->height / 2;
    c->cursor_visible = 1;
    c->last_frame_ms = 0;

    if (ui_init(g_scene, (int32_t)c->width, (int32_t)c->height) != 0)
        return -1;
    if (boot_linux_selftest_enabled())
        (void)linux_user_selftest();
    g_ui_ready = 1;

    compositor_cursor_move(c, c->pointer_x, c->pointer_y);
    compositor_damage_full(c);
    KLOG_INFO("shell: MDE listo (%ux%u)\n", c->width, c->height);
    return 0;
}

void compositor_set_desktop_icons(compositor_t *c,
                                  const mde_desktop_icon_t *icons,
                                  uint32_t count)
{
    (void)c;
    (void)icons;
    (void)count;
}

void compositor_frame_tick(compositor_t *c, uint64_t now_ms)
{
    input_event_t ev;
    uint32_t budget = 0;
    int32_t rx, ry, rw, rh;

    if (!c || !c->ready || !g_ui_ready)
        return;

    input_poll();
    while (budget < COMP_INPUT_BUDGET && input_pop(&ev) == 0) {
        budget++;
        if (ev.type == INPUT_EV_MOUSE_MOVE) {
            c->pointer_x = ev.u.mouse.x;
            c->pointer_y = ev.u.mouse.y;
        }
        ui_event(&ev, now_ms);
    }

    ui_tick(now_ms);
    if (ui_render(&rx, &ry, &rw, &rh))
        compositor_damage_rect(c, rx, ry, rw, rh);

    if (c->cursor_x == c->pointer_x && c->cursor_y == c->pointer_y &&
        !comp_damage_count(&c->damage) && !comp_damage_is_full(&c->damage))
        return;

    /* el sprite tiene alfa: se repinta siempre sobre escena limpia */
    compositor_cursor_move(c, c->pointer_x, c->pointer_y);
    compositor_render(c, comp_scene_copy, c);
    ui_cursor_draw(c->back, c->stride_px, (int32_t)c->width, (int32_t)c->height,
                   c->pointer_x, c->pointer_y);
    compositor_present(c);
    c->last_frame_ms = now_ms;
}

void compositor_flush_now(void)
{
    static int busy;
    int32_t x, y, w, h;

    if (busy || g_comp == NULL || !g_comp->ready || !g_ui_ready)
        return;
    busy = 1;
    ui_invalidate(0, 0, (int32_t)g_comp->width, (int32_t)g_comp->height);
    if (ui_render(&x, &y, &w, &h))
        compositor_damage_rect(g_comp, x, y, w, h);
    else
        compositor_damage_full(g_comp);
    compositor_render(g_comp, comp_scene_copy, g_comp);
    ui_cursor_draw(g_comp->back, g_comp->stride_px, (int32_t)g_comp->width,
                   (int32_t)g_comp->height, g_comp->pointer_x, g_comp->pointer_y);
    compositor_present(g_comp);
    busy = 0;
}

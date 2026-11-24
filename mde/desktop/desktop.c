/*
 * MDE desktop — ciclo de vida del shell (SA-17 / LG-08 Liquid Glass).
 * Colores únicamente vía theme_get / theme_palette / theme_glass (SA-18 / LG-01).
 * Orden pintado escena (contrato LG-03): wallpaper → wm → glass chrome.
 */

#include "mde.h"

#include "../compositor/compositor.h"
#include "../shell/dock.h"

#include "bootsplash.h"
#include "graphics/fb.h"
#include "interrupt/pit.h"
#include "log/klog.h"

#include <stddef.h>
#include <stdint.h>

/* Backbuffer compositor (gfxpayload 1024×768×32). */
#define MDE_COMP_BACK_MAX_PX (1024u * 768u)
static uint32_t     g_comp_back[MDE_COMP_BACK_MAX_PX];
static compositor_t g_compositor;

/* topbar (SA-20): fuerte en mde/shell/topbar.c; no-op si no enlaza */
__attribute__((weak)) void topbar_init(void) {}

__attribute__((weak)) void topbar_set_viewport(int32_t width)
{
    (void)width;
}

/* Wallpaper: implementación en mde/shell/wallpaper.c (SA-21). */
typedef struct wallpaper_image {
    const uint32_t *pixels;
    uint32_t        width;
    uint32_t        height;
    uint32_t        format;
} wallpaper_image_t;

extern int wallpaper_load_default(void);
extern const wallpaper_image_t *wallpaper_get_active(void);
extern void wallpaper_unload(void);

static mde_desktop_state g_desktop;

static void mde_styles_flat_from_palette(const mde_palette *p, mde_desktop_chrome *chrome)
{
    mde_bar_style *bar = &chrome->topbar;
    mde_window_style *win = &chrome->window;

    mde_bar_style_light(bar);
    bar->label = p->text;
    bar->icon  = p->muted;

    mde_window_style_light(win);
    win->title_text = p->text;
}

static void mde_chrome_apply_glass(mde_desktop_chrome *chrome, const mde_palette *p)
{
    mde_bar_glass_style_light(&chrome->topbar, &chrome->topbar_glass);
    chrome->topbar.label = p->text;
    chrome->topbar.icon  = p->muted;

    mde_window_glass_style_light(&chrome->window, &chrome->window_glass);
    chrome->window.title_text = p->text;

    mde_glass_dock_style_light(&chrome->dock_glass);
    mde_glass_menu_style_light(&chrome->menu_glass);
}

static void mde_chrome_apply(mde_desktop_state *st)
{
    const mde_palette *p = st->palette;
    mde_desktop_chrome  *chrome = &st->chrome;

    if (st->config.look & MDE_DESKTOP_LOOK_LIQUID_GLASS) {
        mde_chrome_apply_glass(chrome, p);
    } else {
        mde_styles_flat_from_palette(p, chrome);
    }
}

static void mde_layout_compute(uint32_t w, uint32_t h, mde_desktop_layout *layout,
                               uint32_t components, uint32_t look)
{
    const uint32_t topbar_h = (look & MDE_DESKTOP_LOOK_LIQUID_GLASS)
                                  ? (uint32_t)MDE_CTRL_GLASS_TOPBAR_HEIGHT
                                  : (uint32_t)MDE_CTRL_BAR_HEIGHT;
    const uint32_t dock_pad = MDE_CTRL_DOCK_PADDING;
    const uint32_t dock_margin = MDE_CTRL_DOCK_MARGIN_BOTTOM;
    const uint32_t dock_h = topbar_h + dock_pad * 2u;

    layout->screen = (mde_rect){0, 0, w, h};

    if (components & MDE_COMPONENT_MASK(MDE_COMPONENT_TOPBAR)) {
        layout->topbar = (mde_rect){0, 0, w, topbar_h};
    } else {
        layout->topbar = (mde_rect){0, 0, 0, 0};
    }

    if (components & MDE_COMPONENT_MASK(MDE_COMPONENT_DOCK)) {
        uint32_t dock_w = w > 64u ? w - 64u : w;
        uint32_t dock_x = (w > dock_w) ? (w - dock_w) / 2u : 0u;
        uint32_t dock_y = (h > dock_h + dock_margin) ? h - dock_h - dock_margin : 0u;
        layout->dock = (mde_rect){dock_x, dock_y, dock_w, dock_h};
    } else {
        layout->dock = (mde_rect){0, 0, 0, 0};
    }

    uint32_t work_y = layout->topbar.h;
    uint32_t work_h = (h > work_y) ? h - work_y : 0u;

    if (components & MDE_COMPONENT_MASK(MDE_COMPONENT_DOCK) && layout->dock.h > 0u) {
        uint32_t bottom_inset = (h > layout->dock.y) ? (h - layout->dock.y) + dock_margin : dock_margin;
        if (work_h > bottom_inset) {
            work_h -= bottom_inset;
        } else {
            work_h = 0u;
        }
    }

    layout->work_area = (mde_rect){0, work_y, w, work_h};
}

static int mde_desktop_apply_theme(mde_desktop_state *st)
{
    const mde_theme *theme = theme_get(st->theme_id);
    if (theme == NULL || theme->palette == NULL) {
        return -1;
    }

    st->theme   = theme;
    st->palette = theme_palette(st->theme_id);
    if (st->palette == NULL) {
        return -1;
    }

    mde_chrome_apply(st);
    return 0;
}

static int mde_wallpaper_bootstrap(uint32_t components)
{
    if ((components & MDE_COMPONENT_MASK(MDE_COMPONENT_WALLPAPER)) == 0u) {
        return 0;
    }
    return wallpaper_load_default();
}

static void mde_wallpaper_teardown(uint32_t components)
{
    if ((components & MDE_COMPONENT_MASK(MDE_COMPONENT_WALLPAPER)) == 0u) {
        return;
    }
    wallpaper_unload();
}

/*
 * LG-08: dock_init / topbar_init ya aplican estilo glass en shell (LG-02+).
 * SA-20: Notes / Files / Terminal; topbar 28 px; reloj HH:MM, sin calendario.
 */
static void mde_shell_chrome_bootstrap(uint32_t screen_width, uint32_t screen_height,
                                       uint32_t components)
{
    if (components & MDE_COMPONENT_MASK(MDE_COMPONENT_DOCK)) {
        dock_init();
        dock_set_viewport((int32_t)screen_width, (int32_t)screen_height);
    }

    if (components & MDE_COMPONENT_MASK(MDE_COMPONENT_TOPBAR)) {
        topbar_init();
        topbar_set_viewport((int32_t)screen_width);
    }
}

int mde_init_with_config(uint32_t screen_width, uint32_t screen_height,
                         const mde_desktop_config *config)
{
    mde_desktop_config cfg;

    if (g_desktop.phase == MDE_PHASE_READY || g_desktop.phase == MDE_PHASE_INITIALIZING) {
        return -1;
    }

    if (config != NULL) {
        cfg = *config;
        if (cfg.look == 0u && cfg.theme_id == MDE_LIGHT) {
            cfg.look = MDE_DESKTOP_DEFAULT_LOOK;
        }
    } else {
        cfg.theme_id   = theme_get_default(); /* MDE_LIGHT */
        cfg.components = MDE_DESKTOP_DEFAULT_COMPONENTS;
        cfg.look       = MDE_DESKTOP_DEFAULT_LOOK;
    }

    if (cfg.theme_id != MDE_LIGHT && cfg.theme_id != MDE_DARK) {
        cfg.theme_id = theme_get_default();
    }

    if (cfg.theme_id == MDE_LIGHT && cfg.look == 0u) {
        cfg.look = MDE_DESKTOP_DEFAULT_LOOK;
    }

    g_desktop.phase        = MDE_PHASE_INITIALIZING;
    g_desktop.config       = cfg;
    g_desktop.theme_id     = cfg.theme_id;
    g_desktop.frame_count  = 0u;
    g_desktop.theme        = NULL;
    g_desktop.palette      = NULL;

    if (mde_desktop_apply_theme(&g_desktop) != 0) {
        g_desktop.phase = MDE_PHASE_OFF;
        return -1;
    }

    if (screen_width == 0u || screen_height == 0u) {
        g_desktop.phase = MDE_PHASE_OFF;
        return -1;
    }

    mde_layout_compute(screen_width, screen_height, &g_desktop.layout, cfg.components,
                       cfg.look);

    if (mde_wallpaper_bootstrap(cfg.components) != 0) {
        g_desktop.phase = MDE_PHASE_OFF;
        return -1;
    }

    mde_shell_chrome_bootstrap(screen_width, screen_height, cfg.components);

    g_desktop.phase = MDE_PHASE_READY;
    return 0;
}

int mde_init(uint32_t screen_width, uint32_t screen_height)
{
    return mde_init_with_config(screen_width, screen_height, NULL);
}

void mde_shutdown(void)
{
    if (g_desktop.phase == MDE_PHASE_OFF) {
        return;
    }

    g_desktop.phase = MDE_PHASE_SHUTTING_DOWN;
    mde_wallpaper_teardown(g_desktop.config.components);

    g_desktop.theme   = NULL;
    g_desktop.palette = NULL;
    g_desktop.phase   = MDE_PHASE_OFF;
}

mde_phase_t mde_get_phase(void)
{
    return g_desktop.phase;
}

const mde_desktop_state *mde_desktop_get_state(void)
{
    if (g_desktop.phase != MDE_PHASE_READY && g_desktop.phase != MDE_PHASE_INITIALIZING) {
        return NULL;
    }
    return &g_desktop;
}

const mde_desktop_layout *mde_desktop_get_layout(void)
{
    if (g_desktop.phase != MDE_PHASE_READY) {
        return NULL;
    }
    return &g_desktop.layout;
}

const mde_palette *mde_desktop_palette(void)
{
    if (g_desktop.palette == NULL) {
        return theme_palette(theme_get_default());
    }
    return g_desktop.palette;
}

uint32_t mde_desktop_background_color(void)
{
    const mde_palette *p = mde_desktop_palette();
    return p ? p->bg : theme_palette(theme_get_default())->bg;
}

bool mde_desktop_component_enabled(mde_desktop_component_t component)
{
    if (component >= MDE_COMPONENT_COUNT) {
        return false;
    }
    return (g_desktop.config.components & MDE_COMPONENT_MASK(component)) != 0u;
}

bool mde_desktop_glass_enabled(void)
{
    if (g_desktop.phase != MDE_PHASE_READY) {
        return false;
    }
    return (g_desktop.config.look & MDE_DESKTOP_LOOK_LIQUID_GLASS) != 0u;
}

const mde_desktop_chrome *mde_desktop_get_chrome(void)
{
    if (g_desktop.phase != MDE_PHASE_READY && g_desktop.phase != MDE_PHASE_INITIALIZING) {
        return NULL;
    }
    return &g_desktop.chrome;
}

const mde_glass_tokens *mde_desktop_glass_tokens(void)
{
    if (!mde_desktop_glass_enabled()) {
        return NULL;
    }
    return theme_glass(g_desktop.theme_id);
}

int mde_desktop_tick(void)
{
    if (g_desktop.phase != MDE_PHASE_READY) {
        return -1;
    }
    g_desktop.frame_count++;
    return 0;
}

void mde_desktop_on_ready(void)
{
    if (g_desktop.phase == MDE_PHASE_INITIALIZING) {
        g_desktop.phase = MDE_PHASE_READY;
    }
}

bool mde_desktop_is_ready(void)
{
    return g_desktop.phase == MDE_PHASE_READY;
}

/*
 * Un frame lógico antes del compositor (SA-15); sin calendario ni draw FB.
 */
void mde_frame_once(void)
{
    (void)mde_desktop_tick();
}

/*
 * Punto de entrada kernel (kmain tras fb_init). Enlaza con kernel.c vía extern.
 * Tras éxito: MDE_PHASE_READY; compositor consulta mde_desktop_glass_enabled().
 */
int metta_mde_bringup(uint32_t screen_width, uint32_t screen_height)
{
    int rc = mde_init(screen_width, screen_height);
    if (rc != 0) {
        return rc;
    }
    mde_desktop_on_ready();
    mde_frame_once();
    return mde_desktop_is_ready() ? 0 : -1;
}

void metta_mde_run(void)
{
    const framebuffer_t *fb;
    uint64_t             pixels;
    uint64_t             now_ms;
    int                  shell_ready = 0;

    fb = fb_get();
    if (fb == NULL || !fb->ready || fb->base == NULL) {
        KLOG_ERR("mde_run: framebuffer no listo\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    pixels = (uint64_t)fb->width * (uint64_t)fb->height;
    if (pixels == 0u || pixels > (uint64_t)MDE_COMP_BACK_MAX_PX) {
        KLOG_ERR("mde_run: resolución %ux%u fuera de backbuffer\n",
                 fb->width, fb->height);
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    if (compositor_init(&g_compositor, fb, g_comp_back, NULL) != 0) {
        KLOG_ERR("mde_run: compositor_init falló\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    if (compositor_shell_bootstrap(&g_compositor) != 0) {
        KLOG_ERR("mde_run: shell_bootstrap falló\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    now_ms = pit_ticks_get() * (1000ull / (uint64_t)PIT_TICK_HZ);
    bootsplash_on_mde_ready(now_ms);
    compositor_damage_full(&g_compositor);
    KLOG_INFO("mde_run: bucle shell (fb %ux%u @ phys %llx)\n",
              fb->width, fb->height, (unsigned long long)fb->phys_addr);

    for (;;) {
        now_ms = pit_ticks_get() * (1000ull / (uint64_t)PIT_TICK_HZ);
        bootsplash_tick(now_ms);

        if (bootsplash_phase() == BOOTSPLASH_PHASE_RELEASED) {
            if (!shell_ready) {
                compositor_damage_full(&g_compositor);
                shell_ready = 1;
                KLOG_BOOT("mde_run: desktop paint\n");
            }
            compositor_frame_tick(&g_compositor, now_ms);
        }

        __asm__ volatile("hlt");
    }
}
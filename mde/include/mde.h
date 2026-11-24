#ifndef MDE_H
#define MDE_H

/*
 * MDE — Metta Desktop Environment (SA-17)
 * API pública del shell: ciclo de vida, layout chrome, tema SA-18 y look LG-08.
 * Sin widget de calendario (explícitamente fuera de alcance).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../theme/theme.h"
#include "../theme/controls.h"

#define MDE_CONTRACT_REV 2u

/*
 * Apariencia del escritorio (LG-08). Por defecto Liquid Glass sobre MDE Light.
 */
#define MDE_DESKTOP_LOOK_LIQUID_GLASS (1u << 0u)

#define MDE_DESKTOP_DEFAULT_LOOK MDE_DESKTOP_LOOK_LIQUID_GLASS

/*
 * Orden de pintado escena (LG-03 compositor, back-to-front):
 *   wallpaper → wm → glass chrome
 * Usar mde_desktop_paint_stage_at(i) con i en [0, MDE_DESKTOP_PAINT_STAGE_COUNT).
 */
typedef enum {
    MDE_DESKTOP_PAINT_WALLPAPER = 0,
    MDE_DESKTOP_PAINT_WM,
    MDE_DESKTOP_PAINT_GLASS_CHROME,
    MDE_DESKTOP_PAINT_STAGE_COUNT
} mde_desktop_paint_stage_t;

/* Alias de fase del escritorio (compatibilidad boot-map: mde_init). */
typedef enum {
    MDE_PHASE_OFF = 0,
    MDE_PHASE_INITIALIZING,
    MDE_PHASE_READY,
    MDE_PHASE_SHUTTING_DOWN,
} mde_phase_t;

typedef struct mde_rect {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} mde_rect;

typedef enum {
    MDE_COMPONENT_TOPBAR = 0,
    MDE_COMPONENT_DOCK,
    MDE_COMPONENT_WALLPAPER,
    MDE_COMPONENT_COUNT
} mde_desktop_component_t;

#define MDE_COMPONENT_MASK(c) (1u << (unsigned)(c))

#define MDE_DESKTOP_DEFAULT_COMPONENTS                           \
    (MDE_COMPONENT_MASK(MDE_COMPONENT_TOPBAR)                    \
     | MDE_COMPONENT_MASK(MDE_COMPONENT_DOCK)                      \
     | MDE_COMPONENT_MASK(MDE_COMPONENT_WALLPAPER))

typedef struct mde_desktop_config {
    uint32_t components; /* máscara MDE_COMPONENT_*; nunca incluye calendario */
    mde_theme_id theme_id;
    uint32_t     look;   /* MDE_DESKTOP_LOOK_*; default LIQUID_GLASS */
} mde_desktop_config;

typedef struct mde_desktop_chrome {
    mde_bar_style           topbar;
    mde_window_style        window;
    mde_glass_surface_style topbar_glass;
    mde_glass_dock_style    dock_glass;
    mde_glass_surface_style window_glass;
    mde_glass_menu_style    menu_glass;
} mde_desktop_chrome;

typedef struct mde_desktop_layout {
    mde_rect screen;
    mde_rect topbar;
    mde_rect dock;
    mde_rect work_area;
} mde_desktop_layout;

typedef struct mde_desktop_state {
    mde_phase_t           phase;
    mde_theme_id          theme_id;
    const mde_theme      *theme;
    const mde_palette    *palette;
    mde_desktop_chrome    chrome;
    mde_desktop_layout    layout;
    mde_desktop_config    config;
    uint64_t              frame_count;
} mde_desktop_state;

/* Inicializa escritorio MDE Light vía API de tema SA-18. */
int mde_init(uint32_t screen_width, uint32_t screen_height);

int mde_init_with_config(uint32_t screen_width, uint32_t screen_height,
                         const mde_desktop_config *config);

void mde_shutdown(void);

mde_phase_t mde_get_phase(void);

const mde_desktop_state *mde_desktop_get_state(void);

const mde_desktop_layout *mde_desktop_get_layout(void);

const mde_palette *mde_desktop_palette(void);

uint32_t mde_desktop_background_color(void);

bool mde_desktop_component_enabled(mde_desktop_component_t component);

/* LG-08: true tras MDE_PHASE_READY si look incluye LIQUID_GLASS. */
bool mde_desktop_glass_enabled(void);

const mde_desktop_chrome *mde_desktop_get_chrome(void);

const mde_glass_tokens *mde_desktop_glass_tokens(void);

static inline mde_desktop_paint_stage_t mde_desktop_paint_stage_at(uint32_t index)
{
    if (index >= (uint32_t)MDE_DESKTOP_PAINT_STAGE_COUNT) {
        return MDE_DESKTOP_PAINT_GLASS_CHROME;
    }
    return (mde_desktop_paint_stage_t)index;
}

/*
 * Llamar una vez por frame del compositor (SA-15+).
 * Pinta lógica mínima: solo avanza contador; el draw queda en compositor.
 */
int mde_desktop_tick(void);

/* Señal para bootsplash (SA-21): escritorio listo para retención 2 s. */
void mde_desktop_on_ready(void);

bool mde_desktop_is_ready(void);

/* Bring-up kernel (kmain tras fb_init); deja escritorio READY + primer tick. */
int metta_mde_bringup(uint32_t screen_width, uint32_t screen_height);

/*
 * Bucle del shell: compositor + input. No retorna.
 * Requiere metta_mde_bringup OK y framebuffer listo.
 */
void metta_mde_run(void) __attribute__((noreturn));

#endif /* MDE_H */
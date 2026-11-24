/*
 * Contrato de enlazado escritorio MDE (SA-17 / SA-22) para el compositor.
 */
#ifndef MDE_COMPOSITOR_MDE_H
#define MDE_COMPOSITOR_MDE_H

#include <stdint.h>

#include "../include/mde.h"
#include "../theme/theme.h"

#define MDE_DESKTOP_MAX_ICONS 128u
#define MDE_ICON_HOVER_CHANNEL_BASE 0u

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t id;
} mde_desktop_icon_t;

typedef struct {
    uint8_t     action_id;
    const char *label;
} mde_ctxmenu_item_t;

void mde_interaction_init(void);
void mde_interaction_set_icons(const mde_desktop_icon_t *icons, uint32_t count);
void mde_interaction_pointer_move(int32_t x, int32_t y, uint64_t now_ms);
int32_t mde_interaction_hover_icon_id(void);
int32_t mde_interaction_icon_hover_scale(int32_t icon_id);
void mde_interaction_tick(uint64_t now_ms);

int mde_anim_active(uint32_t channel_id);

void mde_ctxmenu_init(void);
void mde_ctxmenu_close(void);
int mde_ctxmenu_is_open(void);
int mde_ctxmenu_open_at(int32_t screen_x, int32_t screen_y,
                        const mde_ctxmenu_item_t *items, uint32_t count);
void mde_ctxmenu_pointer(int32_t x, int32_t y, int pressed,
                         int32_t screen_w, int32_t screen_h);
int32_t mde_ctxmenu_hovered_index(void);
void mde_ctxmenu_bounds(int32_t *x, int32_t *y, int32_t *w, int32_t *h);
const mde_palette *mde_ctxmenu_theme(void);
uint32_t mde_ctxmenu_item_height(void);
uint32_t mde_ctxmenu_item_count(void);
const char *mde_ctxmenu_item_label(uint32_t index);
int mde_ctxmenu_item_is_separator(uint32_t index);

#endif /* MDE_COMPOSITOR_MDE_H */
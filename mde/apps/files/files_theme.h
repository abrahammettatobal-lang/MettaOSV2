#ifndef MDE_FILES_THEME_H
#define MDE_FILES_THEME_H

#include "../../theme/controls.h"
#include "../../theme/light.h"
#include "../../theme/theme.h"

/*
 * Constantes de layout del explorador — alineadas con MDE Light (SA-18).
 * Topbar/barra de herramientas: 28 px; filas compactas, sin estética “web card”.
 */

#define MDE_FILES_SIDEBAR_WIDTH      200
#define MDE_FILES_TOOLBAR_HEIGHT     MDE_CTRL_BAR_HEIGHT
#define MDE_FILES_LIST_HEADER_HEIGHT 24
#define MDE_FILES_ROW_HEIGHT         26
#define MDE_FILES_PATH_INSET         12
#define MDE_FILES_ICON_COL_WIDTH     28
#define MDE_FILES_SIZE_COL_WIDTH     88
#define MDE_FILES_DATE_COL_WIDTH     120

typedef struct mde_files_chrome {
    mde_window_style window;
    mde_bar_style    toolbar;
    mde_button_style button;
    mde_button_style button_primary;
    uint32_t         sidebar_bg;
    uint32_t         list_bg;
    uint32_t         list_header_bg;
    uint32_t         list_row_hover;
    uint32_t         list_row_selected;
    uint32_t         list_divider;
    uint32_t         folder_tint;
    uint32_t         file_tint;
} mde_files_chrome;

static inline void mde_files_chrome_light(mde_files_chrome *c)
{
    const mde_palette *p = &mde_light_palette;

    mde_window_style_light(&c->window);
    mde_bar_style_light(&c->toolbar);
    mde_button_style_light(&c->button);
    mde_button_style_primary_light(&c->button_primary);

    c->sidebar_bg         = p->secondary;
    c->list_bg            = p->surface;
    c->list_header_bg     = p->bg;
    c->list_row_hover     = p->hover;
    c->list_row_selected  = p->selection;
    c->list_divider       = p->border;
    c->folder_tint        = p->accent;
    c->file_tint          = p->muted;
}

#endif /* MDE_FILES_THEME_H */
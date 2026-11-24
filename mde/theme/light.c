#include "light.h"
#include "glass.h"

const mde_palette mde_light_palette = {
    .surface   = MDE_LIGHT_SURFACE,
    .bg        = MDE_LIGHT_BG,
    .secondary = MDE_LIGHT_SECONDARY,
    .border    = MDE_LIGHT_BORDER,
    .text      = MDE_LIGHT_TEXT,
    .muted     = MDE_LIGHT_MUTED,
    .accent    = MDE_LIGHT_ACCENT,
    .hover     = MDE_LIGHT_HOVER,
    .selection = MDE_LIGHT_SELECTION,
};

const mde_theme mde_light_theme = {
    .id      = MDE_LIGHT,
    .name    = "MDE Light",
    .palette = &mde_light_palette,
    .glass   = &mde_light_glass,
};
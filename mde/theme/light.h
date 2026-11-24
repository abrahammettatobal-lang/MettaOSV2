#ifndef MDE_LIGHT_H
#define MDE_LIGHT_H

#include "theme.h"

/* Conversión 0xRRGGBB → ARGB32 opaco para framebuffer/compositor. */
#define MDE_COLOR_RGB(rgb) ((uint32_t)(0xFF000000u | ((uint32_t)(rgb) & 0xFFFFFFu)))

/*
 * Paleta MDE Light (obligatoria, premium, inspiración macOS con identidad MettaOS).
 * Sin fondos negros ni grises oscuros como base del escritorio.
 */
#define MDE_LIGHT_SURFACE    MDE_COLOR_RGB(0xFFFFFFu)
#define MDE_LIGHT_BG         MDE_COLOR_RGB(0xF6F8FBu)
#define MDE_LIGHT_SECONDARY  MDE_COLOR_RGB(0xEDF1F6u)
#define MDE_LIGHT_BORDER     MDE_COLOR_RGB(0xDCE2EAu)
#define MDE_LIGHT_TEXT       MDE_COLOR_RGB(0x202733u)
#define MDE_LIGHT_MUTED      MDE_COLOR_RGB(0x687385u)
#define MDE_LIGHT_ACCENT     MDE_COLOR_RGB(0x3278D9u)
#define MDE_LIGHT_HOVER      MDE_COLOR_RGB(0xE8F1FFu)
#define MDE_LIGHT_SELECTION  MDE_COLOR_RGB(0xD5E6FFu)

/* Instancia canónica de la paleta clara. */
extern const mde_palette mde_light_palette;
extern const mde_theme    mde_light_theme;

#endif /* MDE_LIGHT_H */
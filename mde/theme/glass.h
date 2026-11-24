#ifndef MDE_GLASS_H
#define MDE_GLASS_H

#include "light.h"
#include <stdint.h>

/*
 * Liquid Glass — macOS Tahoe (Composer 2.5 / LG-T01).
 *
 * Capa translúcida sobre MDE Light: fill perla + borde specular 1 px + blur
 * (compositor) + sombras multicapa. La paleta opaca base sigue en light.h.
 *
 * Contrato ARGB: 0xAARRGGBB (alfa en el byte alto).
 */

#define MDE_COLOR_RGBA(a, rgb) \
    ((uint32_t)(((uint32_t)(a) << 24) | ((uint32_t)(rgb) & 0xFFFFFFu)))

/* --- Fills perlados (alfa real; el fondo debe verse bajo el blur) --- */

/* Paneles flotantes: dock, ventana con foco, overlays principales (~65 %). */
#define MDE_GLASS_FILL_PRIMARY    MDE_COLOR_RGBA(0xA6u, 0xFFFFFFu)

/* Cápsula topbar y barras sobre wallpaper (~58 % tinte escritorio). */
#define MDE_GLASS_FILL_SECONDARY  MDE_COLOR_RGBA(0x94u, 0xF6F8FBu)

/* Píldoras menú, chrome inactivo, hover slots (~62 % secondary MDE). */
#define MDE_GLASS_FILL_CHROME     MDE_COLOR_RGBA(0x9Eu, 0xEDF1F6u)

/* --- Borde specular Tahoe: highlight superior + arista 1 px --- */

/* Halo blanco en el perímetro (luz rasante, ~45 %). */
#define MDE_GLASS_BORDER_SPECULAR MDE_COLOR_RGBA(0x73u, 0xFFFFFFu)

/* Arista estructural suave (#DCE2EA ~35 %; no compite con el specular). */
#define MDE_GLASS_BORDER_BASE     MDE_COLOR_RGBA(0x59u, 0xDCE2EAu)

#define MDE_GLASS_BORDER_WIDTH    1u

/* Franja superior 1 px — brillo directo (~70 % blanco). */
#define MDE_GLASS_SPECULAR_TOP      MDE_COLOR_RGBA(0xB3u, 0xFFFFFFu)
#define MDE_GLASS_SPECULAR_TOP_HPX  1u

/* --- Accent glass (marca Metta #3278D9 + lectura tipo Tahoe System Blue) --- */

#define MDE_GLASS_ACCENT          MDE_COLOR_RGBA(0xD9u, 0x3278D9u)
#define MDE_GLASS_ACCENT_HOVER    MDE_COLOR_RGBA(0xE8u, 0x3278D9u)
#define MDE_GLASS_ACCENT_PRESSED  MDE_COLOR_RGBA(0xB3u, 0x2B6BC4u)

/*
 * Blur — radios de muestreo para el compositor (box/gaussian CPU).
 * Tahoe es más agresivo que LG-01; heavy acotado para uso interactivo.
 */
#define MDE_GLASS_BLUR_LIGHT   16u  /* menús, topbar, pills */
#define MDE_GLASS_BLUR_MEDIUM  36u  /* ventanas, paneles */
#define MDE_GLASS_BLUR_HEAVY   56u  /* dock flotante */

#define MDE_GLASS_SHADOW_LAYERS 3u

typedef struct mde_glass_shadow_layer {
    int8_t   offset_x;
    int8_t   offset_y;   /* Tahoe: desplazamiento Y positivo pequeño */
    uint8_t  blur_radius;
    uint8_t  alpha;      /* 0–255 sobre negro */
} mde_glass_shadow_layer;

typedef struct mde_glass_tokens {
    uint32_t fill_primary;
    uint32_t fill_secondary;
    uint32_t fill_chrome;
    uint32_t border_specular;
    uint32_t border_base;
    uint32_t specular_top;
    uint32_t accent;
    uint32_t accent_hover;
    uint32_t accent_pressed;
    uint8_t  border_width;
    uint8_t  specular_top_height;
    uint8_t  blur_light;
    uint8_t  blur_medium;
    uint8_t  blur_heavy;
    mde_glass_shadow_layer shadow[MDE_GLASS_SHADOW_LAYERS];
} mde_glass_tokens;

extern const mde_glass_tokens mde_light_glass;

#endif /* MDE_GLASS_H */
#include "glass.h"

/*
 * Instancia canónica MDE Light + Liquid Glass (Tahoe).
 * Sombras: contacto → ambient → diffuse (offsets Y bajos, alfa contenida).
 */
const mde_glass_tokens mde_light_glass = {
    .fill_primary   = MDE_GLASS_FILL_PRIMARY,
    .fill_secondary = MDE_GLASS_FILL_SECONDARY,
    .fill_chrome    = MDE_GLASS_FILL_CHROME,
    .border_specular = MDE_GLASS_BORDER_SPECULAR,
    .border_base    = MDE_GLASS_BORDER_BASE,
    .specular_top   = MDE_GLASS_SPECULAR_TOP,
    .accent         = MDE_GLASS_ACCENT,
    .accent_hover   = MDE_GLASS_ACCENT_HOVER,
    .accent_pressed = MDE_GLASS_ACCENT_PRESSED,
    .border_width       = (uint8_t)MDE_GLASS_BORDER_WIDTH,
    .specular_top_height = (uint8_t)MDE_GLASS_SPECULAR_TOP_HPX,
    .blur_light   = (uint8_t)MDE_GLASS_BLUR_LIGHT,
    .blur_medium  = (uint8_t)MDE_GLASS_BLUR_MEDIUM,
    .blur_heavy   = (uint8_t)MDE_GLASS_BLUR_HEAVY,
    .shadow = {
        { 0,  2,  4,  8 },  /* contacto pegado al panel */
        { 0,  6, 10, 14 },  /* penumbra cercana */
        { 0, 12, 20, 20 },  /* halo suave (alfa baja) */
    },
};
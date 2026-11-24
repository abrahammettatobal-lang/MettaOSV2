#include "theme.h"
#include "glass.h"
#include "light.h"

mde_theme_id theme_get_default(void)
{
    return MDE_LIGHT;
}

const mde_theme *theme_get(mde_theme_id id)
{
    switch (id) {
    case MDE_LIGHT:
        return &mde_light_theme;
    case MDE_DARK:
        /* Oscuro no implementado en SA-18; caer al claro para primer arranque seguro. */
        return &mde_light_theme;
    default:
        return &mde_light_theme;
    }
}

const mde_palette *theme_palette(mde_theme_id id)
{
    const mde_theme *t = theme_get(id);
    return t ? t->palette : &mde_light_palette;
}

const mde_glass_tokens *theme_glass(mde_theme_id id)
{
    const mde_theme *t = theme_get(id);
    return (t && t->glass) ? t->glass : &mde_light_glass;
}
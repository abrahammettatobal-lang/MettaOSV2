#ifndef MDE_THEME_H
#define MDE_THEME_H

#include <stdint.h>

struct mde_glass_tokens;
typedef struct mde_glass_tokens mde_glass_tokens;

/* Identificadores de tema. MDE Light es el predeterminado en primer arranque. */
typedef enum {
    MDE_LIGHT = 0,
    MDE_DARK  = 1, /* reservado; modo oscuro solo opcional secundario */
} mde_theme_id;

typedef struct mde_palette {
    uint32_t surface;    /* #FFFFFF — paneles, ventanas */
    uint32_t bg;         /* #F6F8FB — escritorio */
    uint32_t secondary;  /* #EDF1F6 — chrome suave, dock */
    uint32_t border;     /* #DCE2EA — divisores */
    uint32_t text;       /* #202733 — texto principal */
    uint32_t muted;      /* #687385 — etiquetas secundarias */
    uint32_t accent;     /* #3278D9 — acción, foco */
    uint32_t hover;      /* #E8F1FF — fondo hover */
    uint32_t selection;  /* #D5E6FF — selección */
} mde_palette;

typedef struct mde_theme {
    mde_theme_id           id;
    const char            *name;
    const mde_palette     *palette;
    const mde_glass_tokens *glass; /* Liquid Glass; NULL si tema sin capa glass */
} mde_theme;

/* Tema activo por defecto: siempre MDE_LIGHT hasta que el usuario elija otro. */
mde_theme_id theme_get_default(void);

const mde_theme *theme_get(mde_theme_id id);
const mde_palette *theme_palette(mde_theme_id id);
const mde_glass_tokens *theme_glass(mde_theme_id id);

#endif /* MDE_THEME_H */
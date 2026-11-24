# LG-01 — Liquid Glass design system (MDE Light)

**Agente:** `mde-light-design-engineer`  
**Estado:** COMPLETED  
**Ámbito:** `mde/theme/` + este reporte  
**Modelo:** Composer 2.5

## Objetivo

Definir tokens **Liquid Glass** claros (estilo visionOS / macOS liquid glass) como capa sobre la paleta MDE Light obligatoria, con métricas glass para dock, topbar, ventanas y menús, sin sustituir el tema plano legacy.

## Archivos entregados / modificados

| Archivo | Rol |
|---------|-----|
| `mde/theme/glass.h` | Macros ARGB, blur, sombras, `mde_glass_tokens` |
| `mde/theme/glass.c` | Instancia canónica `mde_light_glass` |
| `mde/theme/light.c` | Enlaza `mde_light_theme.glass` |
| `mde/theme/theme.h` | Campo `glass` en `mde_theme`; `theme_glass()` |
| `mde/theme/theme.c` | `theme_glass()`; default sigue `MDE_LIGHT` |
| `mde/theme/controls.h` | Radios glass, estilos dock/topbar/window/menu |

## Paleta base (sin cambios)

Mantiene SA-18: `#FFFFFF`, `#F6F8FB`, `#EDF1F6`, `#DCE2EA`, `#202733`, `#687385`, `#3278D9`, `#E8F1FF`, `#D5E6FF`.

## Tokens Liquid Glass

| Token | Valor | Uso |
|-------|--------|-----|
| `MDE_GLASS_FILL_PRIMARY` | `0xE6FFFFFF` | Paneles glass (dock, ventana) |
| `MDE_GLASS_FILL_SECONDARY` | `0xCCF6F8FB` | Topbar sobre escritorio |
| `MDE_GLASS_FILL_CHROME` | `0xD9EDF1F6` | Variante secondary glass |
| `MDE_GLASS_BORDER_SPECULAR` | `#FFFFFF` @ 40 % (`0x66`) | Halo specular 1 px |
| `MDE_GLASS_BORDER_BASE` | `#DCE2EA` ~70 % | Borde estructural |
| `MDE_GLASS_SPECULAR_TOP` | blanco ~60 % | Franja superior 1 px |
| `MDE_GLASS_ACCENT` | `#3278D9` @ ~80 % | CTA / foco glass |
| Blur light / medium / heavy | 12 / 24 / 48 px | Constantes compositor |
| Sombras (3 capas) | offsets 1/4/12, blur 2/8/20 | Sombra soft multi-layer en `mde_light_glass.shadow[]` |

Helpers: `MDE_COLOR_RGBA(a, rgb)` en `glass.h`.

## Métricas glass (`controls.h`)

| Control | Constante | Valor |
|---------|-----------|-------|
| Topbar | `MDE_CTRL_GLASS_TOPBAR_HEIGHT` | 28 px |
| Dock | `MDE_CTRL_GLASS_DOCK_RADIUS` | 20 px |
| Ventana | `MDE_CTRL_GLASS_WINDOW_RADIUS` | 12 px |
| Menú | `MDE_CTRL_GLASS_MENU_RADIUS` | 10 px |

Blur por superficie: dock `HEAVY`, topbar/menu `LIGHT`, ventana `MEDIUM`.

Estilos inline: `mde_glass_*_style_light()`, `mde_bar_glass_style_light()`, `mde_window_glass_style_light()`.

## API de tema

```c
mde_theme_id theme_get_default(void);              /* MDE_LIGHT */
const mde_glass_tokens *theme_glass(mde_theme_id);   /* &mde_light_glass para Light */
```

`mde_light_theme` incluye `.glass = &mde_light_glass`.

## Integración pendiente

- Makefile: compilar `mde/theme/glass.c`.
- Compositor: consumir `blur_*` y `shadow[]` al dibujar superficies glass.
- Shell dock/topbar: sustituir fill opaco por helpers glass cuando LG-02+ lo indique.

## Pruebas

```bash
gcc -std=c11 -Wall -Wextra -c mde/theme/glass.c -o /tmp/glass.o
gcc -std=c11 -Wall -Wextra -c mde/theme/light.c -o /tmp/light.o
gcc -std=c11 -Wall -Wextra -c mde/theme/theme.c -o /tmp/theme.o
```

Verificación: `theme_get_default() == MDE_LIGHT`, `theme_glass(MDE_LIGHT)->fill_primary == MDE_GLASS_FILL_PRIMARY`.

## Criterios de aceptación

- [x] Tokens glass fill, borde specular, highlight superior
- [x] Niveles blur light/medium/heavy
- [x] Sombras multi-capa en `mde_glass_tokens`
- [x] Accent glass con alfa
- [x] Paleta MDE Light intacta
- [x] `controls.h` ampliado (radios/altura glass)
- [x] `theme_get_default()` → `MDE_LIGHT` con capa glass asociada
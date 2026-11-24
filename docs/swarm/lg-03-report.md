# LG-03 — Compositor Liquid Glass (Composer 2.5)

**Agente:** compositor-performance-engineer  
**Estado:** COMPLETED  
**Ámbito:** `mde/compositor/`

## Objetivo

Escritorio con chrome **Liquid Glass**: blur frosted del fondo, tint semitransparente (`mde_light_glass`), borde specular, **Z-order** correcto, **dirty rects** solo en chrome/cursor cuando el escritorio es estático, y **caché frosted** para dock/topbar.

## Archivos nuevos / tocados

| Archivo | Rol |
|---------|-----|
| `compositor_glass.h` / `compositor_glass.c` | Box-blur CPU, tint, specular, caché por slot (topbar/dock) |
| `compositor_paint.c` | Draw lists SA-20 → material glass (`theme_glass`) |
| `compositor_frame.c` | Z-order escena, wallpaper weak, ventanas weak, menú glass |
| `compositor.c` | Invalidación de caché en `damage_full` / `begin_frame` / `shutdown` |
| `compositor_paint.h` | Firmas paint con `mde_glass_tokens` |

## Pipeline de pintado (Z-order)

Por cada dirty rect en `comp_scene_draw`:

1. **Wallpaper** — `wallpaper_get_active()` (weak; fallback color escritorio SA-17).
2. **Ventanas** — `comp_paint_windows_region` (weak; SA-19 cuando enlaza WM).
3. **Glass chrome** — topbar + dock vía `comp_paint_chrome` (blur / caché).
4. **Menú contextual** — `comp_glass_paint_menu`.
5. **Cursor** — tras `compositor_render`, `compositor_cursor_paint` (SA-15).

## Liquid Glass

- Tokens: `theme_glass(theme_get_default())` → `mde/theme/glass.h`.
- Panel dock: `blur_medium`, borde redondeado + stroke `border_base`.
- Topbar: `blur_light`, panel rectangular full-width.
- Comandos `kind == 1` (borde plano SA-20) se omiten; el specular lo aplica `comp_glass_apply_specular`.
- Sin API blur en `mde/gfx` — implementación local en `compositor_glass.c` (box-blur 2×, radio acotado).

## Caché frosted

| Slot | Buffer estático | Invalidación |
|------|-----------------|--------------|
| `COMP_GLASS_SLOT_TOPBAR` | 1920×40 px | `damage_full`, viewport, dirty bajo topbar |
| `COMP_GLASS_SLOT_DOCK` | 640×120 px | idem dock |

Si geometría coincide y `valid`, `comp_glass_paint_panel` hace blit desde caché (sin re-blur). Invalidación extra: entrada del cursor en topbar/dock (`comp_track_glass_cursor_invalidate`), `damage_full`, resize/bootstrap.

## Dirty rects y frame tick

`compositor_frame_tick` (sin cambio de contrato SA-15):

- Ratón en área de trabajo → típicamente **1–2 rects** (cursor).
- Chrome topbar/dock → seed **una vez** (`comp_damage_chrome_seed`).
- Hover iconos / menú contextual → rects acotados al control.
- **Sin** `present full_frame` tras el arranque si el puntero no fuerza invalidación global.

## Build

Añadir a `MDE_COMPOSITOR_SRCS`:

```makefile
mde/compositor/compositor_glass.c
```

Includes existentes (`-Imde/theme`) cubren `glass.h`.

## QEMU — verificación

- Mover cursor por el escritorio: logs `[comp] r=1` o `r=2`, sin `present full_frame`.
- Topbar/dock visibles con fondo difuminado a través del panel (wallpaper SA-21 si enlazado).
- Dock: sombra tenue (alpha bajo draw list) bajo panel glass.

## Criterios LG-03

- [x] Frosted blur + tint + specular en topbar/dock/menús
- [x] `compositor_frame_tick` + dirty parcial (chrome/cursor)
- [x] Z-order wallpaper → ventanas → glass → cursor
- [x] Caché frosted dock/topbar estático
- [x] Draw lists dock/topbar conectadas a material glass
# LG-06 — Menús / Launcher Liquid Glass + motion

| Campo | Valor |
|-------|-------|
| Misión | LG-06 overlay glass, animaciones entrada, selección escritorio |
| Agente | `desktop-interaction-engineer` |
| Modelo | Composer 2.5 |
| Estado | COMPLETED |
| Fecha | 2026-11-22 |
| Alcance | Solo archivos listados en la misión (sin `desktop.c` ni compositor) |

## Objetivo

Aplicar **Liquid Glass** y **motion** no bloqueante al menú contextual, al launcher MDE y a la banda de selección del escritorio, reutilizando tokens LG-01 (`mde_light_glass`, `mde_glass_menu_style_light`).

## Archivos entregados

| Archivo | Rol |
|---------|-----|
| `mde/desktop/animations.c` | Canales 60–63; fade + scale 140 ms ease-out cúbico (rango 120–160 ms) |
| `mde/desktop/context_menu.c` | Panel glass, radio 10, hover `#E8F1FF`, atajos muted (`\t` en etiqueta) |
| `mde/desktop/interaction.c` | Rubber-band selección ARGB `#D5E6FF` @ ~60 % |
| `mde/apps/launcher.c` | Overlay centrado, barra búsqueda glass, rejilla apps GUI |
| `mde/apps/launcher.h` | API manifiesto + UI overlay |
| `docs/swarm/lg-06-report.md` | Este informe |

## Menú contextual (glass)

| Aspecto | Implementación |
|---------|----------------|
| Superficie | `mde_glass_menu_style_light` → fill primario, blur light, borde specular |
| Radio | `MDE_CTRL_GLASS_MENU_RADIUS` (10 px) |
| Hover fila | `palette->hover` (`#E8F1FF`) vía `mde_ctxmenu_item_hover_fill()` |
| Atajos | Texto tras `\t` en `label`; color `muted` (`#687385`) |
| Motion | `mde_ctxmenu_tick(now_ms)` arma `mde_anim_ctxmenu_enter` al abrir |
| Métricas anim | opacidad 0→1, escala 0.94→1.0, 140 ms, ease-out |

API glass / paint (compositor futuro): `mde_ctxmenu_glass_style`, `mde_ctxmenu_panel_*`, `mde_ctxmenu_open_opacity/scale`, `mde_ctxmenu_item_shortcut`, `mde_ctxmenu_shortcut_x`.

## Launcher overlay

| Aspecto | Valor |
|---------|--------|
| Panel | 480×520 px centrado; glass ventana (radio 10) |
| Búsqueda | 36 px alto, `MDE_GLASS_FILL_CHROME`, blur menu |
| Rejilla | 4 columnas × 96 px; solo apps `kind=gui`; filtro case-insensitive |
| Motion | `mde_launcher_ui_tick` → mismos canales/curva que menú |

API: `mde_launcher_ui_open/close/tick`, `mde_launcher_ui_layout`, `mde_launcher_ui_panel_glass`, `mde_launcher_ui_search_glass`, `mde_launcher_ui_visible_entry`.

## Selección escritorio

- `mde_interaction_selection_begin/update/end` + `mde_interaction_selection_bounds`
- Relleno: `MDE_COLOR_RGBA(0x99, 0xD5E6FF)`; borde acento glass `0xCC3278D9`
- No bloquea hover de iconos (estados ortogonales)

## Animaciones (canales)

| Canal | Uso |
|-------|-----|
| 0–59 | Hover escala iconos (máx. `icon_id` 59 por límite de 64 canales) |
| 60 | Opacidad menú contextual |
| 61 | Escala menú contextual |
| 62 | Opacidad launcher |
| 63 | Escala launcher |

Helpers: `mde_anim_overlay_enter/dismiss`, `mde_anim_ctxmenu_*`, `mde_anim_launcher_*`.

## Integración pendiente (fuera de alcance)

| Consumidor | Acción |
|------------|--------|
| Compositor paint | Usar glass + opacity/scale al dibujar menú/launcher; atajos muted |
| `compositor_frame.c` | `mde_ctxmenu_tick`, `mde_launcher_ui_tick` por frame |
| Input | Rubber-band con botón primario en fondo escritorio |

## Pruebas sugeridas

```bash
gcc -std=c11 -Wall -Wextra -c mde/desktop/animations.c -o /tmp/anim.o
gcc -std=c11 -Wall -Wextra -I mde/theme -c mde/desktop/context_menu.c -o /tmp/ctx.o
gcc -std=c11 -Wall -Wextra -I mde/theme -c mde/desktop/interaction.c -o /tmp/inter.o
gcc -std=c11 -Wall -Wextra -I mde/theme -I mde/apps -c mde/apps/launcher.c -o /tmp/launch.o
```

1. Abrir menú → `mde_ctxmenu_open_opacity()` crece de 0 a 1 en ~140 ms con `mde_ctxmenu_tick`.
2. Etiqueta `"Paste\tCtrl+V"` → label `Paste`, shortcut `Ctrl+V`, color muted.
3. Launcher con query `"not"` → una entrada `notes`.
4. Selección arrastrada → bounds normalizados y fill `#D5E6FF` semitransparente.

## Criterios de aceptación

- [x] Panel menú glass (blur+tint), radio 10, hover `#E8F1FF`, atajos muted
- [x] Fade+scale 120–160 ms ease-out tick-based (menú + launcher)
- [x] Launcher overlay glass centrado, search bar, grid apps
- [x] Selección desktop rect glass `#D5E6FF` con alfa
- [x] Reporte LG-06 en `docs/swarm/`
# SA-15 — Compositor MDE (dirty rects, doble buffer, present parcial)

**Agente:** compositor-performance-engineer  
**Estado:** COMPLETED (integración chrome SA-20, entrada SA-13, gfx SA-16)  
**Ámbito:** `mde/compositor/`

## Objetivo

Compositor con **back buffer CPU**, **damage rects**, **present parcial** (SA-14) y **bucle de frame** que conecta escritorio, shell chrome e input **sin redibujar la pantalla completa al mover el ratón**.

## Archivos

| Archivo | Rol |
|---------|-----|
| `dirty.h` / `dirty.c` | `comp_damage_t`, merge, tope 64 → `full_frame` |
| `compositor.h` / `compositor.c` | Doble buffer, cursor, render, present, stats |
| `compositor_frame.c` | `input_poll`/`input_pop`, `mde_desktop_tick`, interacción SA-22 |
| `compositor_paint.c` | Chrome SA-20 + **texto SA-16** (`glyph_cache_draw_string`) |
| `compositor_shell.h` | Contrato `dock_*` / `topbar_*` draw lists y etiquetas |
| `compositor_mde.h` | `mde_desktop_*`, interacción, menú contextual |

## Bucle de frame

```c
compositor_shell_bootstrap(c);  /* glyph_cache_init, dock/topbar, SA-22 */
compositor_frame_tick(c, now_ms);
```

Por frame:

1. `input_poll()` + drenar cola (`input_pop`, presupuesto 64).
2. Ratón → `compositor_cursor_move` (solo rects del cursor) + `mde_interaction_pointer_move`.
3. `mde_desktop_tick()` + `mde_interaction_tick(now_ms)`.
4. Dirty parcial: hover de iconos, menú contextual, chrome inicial (topbar + dock una vez).
5. `compositor_render(comp_scene_draw)` — repinta **solo** rects dañados.
6. `compositor_present()` — `fb_blit` parcial + `fb_present()`.

**Sin full redraw por ratón:** el movimiento del puntero invalida típicamente 2 rects (cursor viejo + nuevo). Topbar/dock/texto solo entran si el dirty rect intersecta su geometría.

## Chrome + texto (SA-16 / SA-20)

`comp_paint_chrome()` en regiones dañadas:

1. `topbar_build_draw_list` / `dock_build_draw_list` → rects MDE Light.
2. `comp_paint_chrome_text()` → `glyph_cache_draw_string`:
   - Topbar: marca «Metta», app activa, menús, reloj `HH:MM`, «Cfg».
   - Dock: etiqueta por ícono 40×40 si existe `dock_item_label()` (enlace fuerte en `dock.c`; stub débil si falta).
3. Menú contextual: rects + etiquetas por fila (SA-22).

La caché de glifos (64 entradas) evita re-rasterizar cadenas estáticas en cada repintado de chrome acotado por clip.

## API resumida

```c
compositor_init(c, fb, back, &cfg);
mde_init(fb->width, fb->height);
compositor_shell_bootstrap(c);
compositor_set_desktop_icons(c, icons, n);  /* opcional */
compositor_frame_tick(c, now_ms);
```

## Stats QEMU

`compositor_stats_t.last_present_count`: con ratón en área de trabajo, valores bajos (`1`–`3`); sin `present full_frame` tras el primer frame.

`log_dirty_counts` + `log_fn` → líneas `[comp] r=N d=M`.

## Build (SA-03)

```makefile
MDE_COMPOSITOR_SRCS := mde/compositor/dirty.c mde/compositor/compositor.c \
                       mde/compositor/compositor_paint.c mde/compositor/compositor_frame.c
MDE_GFX_SRCS := mde/gfx/draw.c mde/gfx/font.c mde/gfx/glyph_cache.c
CFLAGS += -Ikernel/include -Imde/include -Imde/gfx
```

## Criterios de aceptación

- [x] Back buffer + present parcial
- [x] Frame tick con SA-13 / SA-17 / SA-20 / SA-22
- [x] Texto topbar/dock vía `glyph_cache_draw_string` en pasada chrome
- [x] Ratón sin invalidar framebuffer completo
- [x] Reporte actualizado

## Fuera de alcance

- Ventanas WM (SA-16 wm)
- `dock_item_label` en `dock.c` (recomendado para etiquetas reales en dock)
- Tipografía avanzada / Unicode (SA-16 fase mínima 8×8)
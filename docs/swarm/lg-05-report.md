# LG-05 — Ventanas Liquid Glass (Composer 2.5)

| Campo | Valor |
|-------|-------|
| Misión | LG-05 Ventanas Liquid Glass |
| Agente | window-manager-engineer |
| Estado | COMPLETED |
| Fecha | 2026-11-22 |
| Ámbito | `mde/wm/` + este reporte |

## Objetivo

Chrome de ventana **Liquid Glass** claro sobre tokens LG-01: titlebar glass 28 px, cliente `#FFFFFF` sólido, borde specular, sombra soft multi-capa, radio 12 px, foco activo/inactivo, controles compactos con hover glass y maximizado dentro del área útil (topbar 28 px + dock).

Las firmas existentes de `wm_*` en `window.h` se conservan; se añaden entradas nuevas documentadas abajo.

## Archivos

| Archivo | Cambio |
|---------|--------|
| `mde/wm/chrome.h` | Capa glass, draw list, controles, API pintado |
| `mde/wm/chrome.c` | Estilos `mde_window_glass_style_light`, sombras `mde_light_glass.shadow[]` |
| `mde/wm/window.h` | `chrome_ctrl_hover`, viewport / maximize / chrome pointer |
| `mde/wm/window.c` | Work area alineada con `mde/desktop`, estados ventana |
| `mde/wm/focus.c` | Sin cambios (invalidación en foco ya compatible) |

## Apariencia

| Elemento | Token / métrica |
|----------|-----------------|
| Titlebar (foco) | `MDE_GLASS_FILL_PRIMARY` + blur `MDE_CTRL_GLASS_WINDOW_BLUR` (24 px) |
| Titlebar (sin foco) | `MDE_GLASS_FILL_CHROME` (más opaco) |
| Cliente | `#FFFFFF` (`MDE_COLOR_RGB(0xFFFFFF)`) |
| Borde | `border_base` + halo `border_specular` 1 px |
| Specular top | `MDE_GLASS_SPECULAR_TOP`, 1 px |
| Radio marco | `MDE_CTRL_GLASS_WINDOW_RADIUS` (12 px) |
| Altura título | `MDE_CTRL_GLASS_TOPBAR_HEIGHT` (28 px) |
| Sombra | 3 capas `mde_light_glass.shadow[]` |
| Controles | 12×12 px, gap 5 px, hover glass (`blur_light`, fill blanco ~80 %) |

## API nueva (LG-05)

```c
void wm_set_viewport(int32_t width, int32_t height);
void wm_work_area_set(const mde_rect_t *area);  /* NULL = recálculo automático */
mde_rect_t wm_work_area_get(void);

int wm_window_minimize(uint32_t id);
int wm_window_maximize(uint32_t id);
int wm_window_restore(uint32_t id);
int wm_window_toggle_maximize(uint32_t id);

int wm_chrome_pointer_move(uint32_t id, int32_t px, int32_t py);
int wm_chrome_control_click(uint32_t id, int32_t px, int32_t py);

int wm_chrome_build_draw_list(const mde_window_t *win, uint32_t focused_id,
                              mde_wm_chrome_draw_list_t *out);
```

## Área útil y maximizar

`wm_work_area_recompute()` replica la lógica de `mde_layout_compute` en `mde/desktop/desktop.c`:

- Top inset: `MDE_CTRL_GLASS_TOPBAR_HEIGHT` (28 px).
- Bottom inset: panel dock (`28 + 2×padding`) + `MDE_CTRL_DOCK_MARGIN_BOTTOM`.

`wm_window_maximize` guarda `restore_bounds`, fija `MDE_WIN_STATE_MAXIMIZED` y ajusta `bounds` (solo cliente) a `work_area − title_height`. Al cambiar viewport, las ventanas maximizadas se reajustan.

## Controles y arrastre

Orden derecha→izquierda: cerrar, maximizar, minimizar. `wm_move_begin` rechaza drags que empiecen sobre un control (`-3`). `wm_chrome_control_click` enlaza destroy / minimize / toggle maximize.

## Compositor (integración pendiente)

Consumir `wm_chrome_build_draw_list` y respetar `blur_radius` por comando. `wm_chrome_collect_damage` incluye rects de sombra glass para dirty regions.

## Prueba host

```bash
gcc -std=c11 -Wall -Wextra -c mde/theme/glass.c -I mde/theme -o /tmp/glass.o
gcc -std=c11 -Wall -Wextra -c mde/wm/chrome.c -I mde -I mde/theme -o /tmp/chrome.o
gcc -std=c11 -Wall -Wextra -c mde/wm/window.c -I mde -I mde/theme -o /tmp/window.o
gcc -std=c11 -Wall -Wextra -c mde/wm/focus.c -I mde -I mde/theme -o /tmp/focus.o
```

Flujo sugerido: `wm_init()`, `wm_set_viewport(1920, 1080)`, `wm_window_create(...)`, `wm_window_maximize(id)`, comprobar `wm_window_client_rect` dentro de 28 px superior y encima del dock.

## Fuera de alcance

- Blur real en framebuffer (compositor SA-15).
- Makefile / enlazado kernel.
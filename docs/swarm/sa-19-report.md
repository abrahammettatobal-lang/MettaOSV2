# SA-19 — Window Manager (ventanas, foco, chrome)

| Campo | Valor |
|-------|-------|
| Misión | SA-19 Window manager |
| Agente | window-manager-engineer |
| Estado | COMPLETED (stubs + contratos) |
| Fecha | 2026-11-22 |
| Ámbito | `mde/wm/` únicamente |

## Objetivo

Gestor de ventanas MDE Light: superficie blanca / gris perla en chrome, barra de título compacta (28 px), sombra de baja opacidad, **Z-order**, **foco**, y stubs de **mover / redimensionar** con reutilización de `surface_token` en traslaciones (contrato compositor SA-15).

## Archivos entregados

| Archivo | Rol |
|---------|-----|
| `mde/wm/window.h` | Tipos `mde_window_t`, rect, interacción, `mde_compositor_ops_t`, API pública |
| `mde/wm/window.c` | Pool fijo (64), creación/destrucción, stack Z, move/resize stub |
| `mde/wm/focus.h` / `focus.c` | Foco, ciclo Alt+Tab stub (`wm_focus_next` / `prev`), foco al clic |
| `mde/wm/chrome.h` / `chrome.c` | Estilo Light vía `mde_window_style_light`, geometría marco/sombra/título |

## Tema y apariencia

Integración con SA-18 (`mde/theme/controls.h`):

| Elemento | Token / métrica |
|----------|-----------------|
| Cuerpo ventana | `surface` (#FFFFFF) |
| Barra de título (sin foco) | `secondary` (#EDF1F6 — gris perla) |
| Barra con foco | `surface` (más limpia) |
| Borde | `border`, 1 px |
| Radio | 10 px (`MDE_CTRL_WINDOW_RADIUS`) |
| Sombra | α ≈ 24 (`MDE_CTRL_WINDOW_SHADOW_ALPHA`), offset (0, 4) |
| Título | altura 28 px |

## Modelo de geometría

`mde_window.bounds`: esquina superior izquierda de la **barra de título**, `w` = ancho del marco, `h` = alto solo del **cliente**. El compositor usa `wm_chrome_frame_rect` para el rectángulo total (cliente + título + sombra).

## Z-order

- Cada ventana tiene `z_order` monotónico (`g_z_counter`).
- `wm_window_raise` → nuevo máximo.
- `wm_window_lower` → intercambio con la ventana inmediatamente inferior, o `z_order = 0` si ya es la más baja.
- `wm_window_stack_bottom_to_top` entrega IDs ordenados para el compositor (pintar fondo → frente).
- `wm_window_topmost_id` para hit-testing simple.

## Foco

- `wm_focus_set` / `clear` / `get`.
- Al crear ventana: foco + raise.
- `wm_focus_on_click`: raise + foco (entrada ratón futura).
- Cambio de foco invalida titlebar de ventana anterior y nueva vía `wm_window_invalidate`.

## Move / resize (stub)

| API | Comportamiento |
|-----|----------------|
| `wm_move_begin` | Solo si puntero en `wm_chrome_titlebar_rect` |
| `wm_move_update` | Actualiza `bounds`; llama `surface_translate` si hay ops compositor (reutiliza superficie) |
| `wm_move_end` | Limpia interacción |
| `wm_resize_*` | Requiere `MDE_WIN_FLAG_RESIZABLE`; mínimos 120×80; sin realloc de superficie (invalidación hasta SA-15) |

Estado global: `wm_interaction_get()`.

## Contrato compositor (SA-15)

```c
typedef struct mde_compositor_ops {
    void (*invalidate_rect)(const mde_rect_t *rect, void *ctx);
    uint32_t (*surface_create)(uint32_t w, uint32_t h, void *ctx);
    void (*surface_destroy)(uint32_t token, void *ctx);
    void (*surface_translate)(uint32_t token, int32_t dx, int32_t dy, void *ctx);
    void *ctx;
} mde_compositor_ops_t;
```

Registrar con `wm_set_compositor_ops` antes de crear ventanas. Sin ops: lógica geométrica sigue siendo válida; no hay I/O de framebuffer.

`wm_chrome_collect_damage` devuelve rects (titlebar, cuerpo, sombra) para el primer frame del compositor.

## Inicialización

```c
wm_init();  /* chrome + foco + pool */
/* opcional: wm_set_compositor_ops(&ops); */
```

## Fuera de alcance

- Kernel, framebuffer (`kernel/graphics/fb.c`), input PS/2.
- Compositor completo (SA-15), WM shell, animaciones.
- Makefile: añadir objetos `mde/wm/*.c` y `-I$(ROOT)/mde/theme` en misión SA-03.

## Verificación sugerida (host, no freestanding)

```bash
gcc -std=c11 -Wall -Wextra -c mde/theme/light.c -I mde/theme -o /tmp/light.o
gcc -std=c11 -Wall -Wextra -c mde/wm/chrome.c -I mde/wm -I mde/theme -o /tmp/chrome.o
gcc -std=c11 -Wall -Wextra -c mde/wm/focus.c -I mde/wm -I mde/theme -o /tmp/focus.o
gcc -std=c11 -Wall -Wextra -c mde/wm/window.c -I mde/wm -I mde/theme -o /tmp/window.o
```

Prueba manual (enlace ligero con `main` de prueba local): crear dos ventanas, `wm_window_raise` / `lower`, `wm_move_begin` + `wm_move_update`, comprobar que `surface_translate` se invoca con deltas acumulados desde el inicio del drag.

## Criterios de aceptación

- [x] Ventanas tema claro (blanco / gris perla, sombra sutil)
- [x] Z-order raise / lower / stack
- [x] Foco y actualización visual de titlebar (stub damage)
- [x] Move con `surface_translate`; resize con límites mínimos
- [x] Solo `mde/wm/` + este informe
- [x] Sin cambios en kernel

## Dependencias y siguientes pasos

| Misión | Acción |
|--------|--------|
| SA-15 | Implementar ops compositor, blit chrome + contenido |
| SA-16 / input | Cablear `wm_move_*` / `wm_resize_*` a eventos puntero |
| SA-03 | Incluir fuentes WM en build MDE |
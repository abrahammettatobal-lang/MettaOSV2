# SA-22 — Interacción de escritorio (hover, animaciones, menú contextual)

**Agente:** `desktop-interaction-engineer`  
**Estado:** COMPLETED  
**Ámbito:** `mde/desktop/` (solo los tres `.c` de esta misión)

## Objetivo

Hover **estable** en iconos del escritorio (sin parpadeo al pasar entre vecinos), animaciones **no bloqueantes** basadas en tiempo monotónico, y menú contextual del escritorio alineado con **MDE Light** (SA-18).

## Archivos entregados

| Archivo | Rol |
|---------|-----|
| `mde/desktop/animations.c` | Hasta 64 canales; `mde_anim_tick(now_ms)` avanza easing sin sleeps |
| `mde/desktop/interaction.c` | Hit-test de iconos, retardo de cambio de hover, gracia al salir, escala animada |
| `mde/desktop/context_menu.c` | Apertura/cierre, hit-test de ítems, acotado a pantalla, paleta vía `theme_palette()` |

## Hover estable (iconos)

| Parámetro | Valor | Efecto |
|-----------|-------|--------|
| `MDE_HOVER_SWITCH_DELAY_MS` | 48 | Cambiar de icono solo si el puntero permanece en el nuevo destino ese tiempo |
| `MDE_HOVER_LEAVE_GRACE_MS` | 90 | Mantener hover actual al salir brevemente del rectángulo |
| Escala en hover | 1.0 → 1.08 | Canal de animación por `icon_id` (120 ms, ease-out cúbico) |

Flujo por frame:

1. `mde_interaction_pointer_move(x, y, now_ms)` en eventos de ratón.
2. `mde_interaction_tick(now_ms)` una vez por frame (también dispara `mde_anim_tick`).

## Animaciones no bloqueantes

- **Sin** `sleep`, bucles de espera ni bloqueo del hilo del compositor.
- `mde_anim_start(channel, from, to, duration_ms, ease_id, now_ms)` programa un canal.
- `mde_anim_tick(now_ms)` interpola todos los canales activos y devuelve cuántos siguen corriendo.
- Reintentar el mismo objetivo en un canal ya activo es idempotente (evita reinicios por ruido de puntero).

## Menú contextual

- Máximo **16** ítems; altura de fila **28 px** (coherente con `MDE_CTRL_BAR_HEIGHT`).
- Separadores: `action_id == 255`.
- `mde_ctxmenu_open_at` relayout por ancho de etiqueta; `mde_ctxmenu_pointer` hace hover y “click” al soltar con índice bajo el cursor.
- Colores: lectura de `mde_palette` del tema por defecto (`mde_ctxmenu_theme()`); el dibujo lo hace el compositor (SA-15).

## API pública (pendiente de headers en SA-03 / shell)

Símbolos exportados sin `.h` en esta misión — el enlazado del módulo MDE debe declararlos o generar `mde/desktop/*.h` en una pasada siguiente.

**Animaciones:** `mde_anim_init`, `mde_anim_start`, `mde_anim_stop`, `mde_anim_set_immediate`, `mde_anim_value`, `mde_anim_active`, `mde_anim_tick`.

**Interacción:** `mde_interaction_init`, `mde_interaction_set_icons`, `mde_interaction_pointer_move`, `mde_interaction_hover_icon_id`, `mde_interaction_icon_hover_scale`, `mde_interaction_tick`.

**Menú:** `mde_ctxmenu_init`, `mde_ctxmenu_open_at`, `mde_ctxmenu_close`, `mde_ctxmenu_is_open`, `mde_ctxmenu_pointer`, `mde_ctxmenu_hovered_index`, `mde_ctxmenu_take_action`, `mde_ctxmenu_bounds`, `mde_ctxmenu_theme`, `mde_ctxmenu_item_*`.

Tipo de icono (definido en `interaction.c`, duplicar en header al integrar):

```c
typedef struct {
    int32_t x, y, w, h;
    int32_t id;
} mde_desktop_icon_t;
```

## Integración pendiente

| Dependencia | Nota |
|-------------|------|
| SA-03 Makefile | Añadir los tres `.c` y `-I mde/theme` para `context_menu.c` + enlazar `theme.c` / `light.c` |
| SA-15 compositor | Llamar tick por frame; leer escala/hover para dirty rects |
| Input (PS/2) | Reenviar move/botón a `mde_interaction_*` y menú contextual en botón secundario |

## Pruebas sugeridas (WSL / gcc)

```bash
gcc -std=c11 -Wall -Wextra -c mde/desktop/animations.c
gcc -std=c11 -Wall -Wextra -c mde/desktop/interaction.c
gcc -std=c11 -Wall -Wextra -I mde/theme -c mde/desktop/context_menu.c
gcc -std=c11 -Wall -Wextra -I mde/theme -c mde/theme/theme.c mde/theme/light.c
```

Verificación lógica:

1. Puntero oscila A↔B en &lt;48 ms → hover permanece en A.
2. Tras 48 ms en B → hover pasa a B y escala de A vuelve a 1.0 sin saltos.
3. Salida rápida del icono &lt;90 ms → hover conservado; tras gracia → `-1`.
4. `mde_anim_tick` con `now_ms` fijo no altera valores (no avanza el tiempo).

## Criterios de aceptación

- [x] Tres archivos en `mde/desktop/` según alcance
- [x] Hover con retardo y gracia (estabilidad en iconos)
- [x] Animaciones solo vía tick monotónico
- [x] Menú contextual con hit-test y tema MDE Light
- [x] Reporte SA-22 en `docs/swarm/`

## Fuera de alcance

- Drag & drop, animaciones de ventana, menús de aplicación
- Headers públicos y cableado en shell/compositor
- Renderizado en framebuffer (solo estado y métricas para el compositor)
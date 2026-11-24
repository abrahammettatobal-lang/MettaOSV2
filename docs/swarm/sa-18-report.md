# SA-18 — MDE Light UI (sistema de tema centralizado)

**Agente:** `mde-light-design-engineer`  
**Estado:** COMPLETED  
**Ámbito:** `mde/theme/` (sin cambios en kernel)

## Objetivo

Establecer el tema claro **MDE Light** como predeterminado desde el primer arranque, con paleta y estilos de controles reutilizables por shell, compositor y apps nativas.

## Archivos entregados

| Archivo | Rol |
|---------|-----|
| `mde/theme/theme.h` | Tipos `mde_theme_id`, `mde_palette`, `mde_theme`; API pública |
| `mde/theme/light.h` | Macros de paleta obligatoria + instancias `extern` |
| `mde/theme/light.c` | `mde_light_palette`, `mde_light_theme` |
| `mde/theme/theme.c` | `theme_get_default()` → `MDE_LIGHT`; resolución de tema |
| `mde/theme/controls.h` | Estilos botón, barra (topbar 28 px), ventana |

## Paleta (obligatoria)

| Token | Hex | Uso |
|-------|-----|-----|
| surface | `#FFFFFF` | Ventanas, paneles |
| bg | `#F6F8FB` | Escritorio |
| secondary | `#EDF1F6` | Chrome, dock, barra de título |
| border | `#DCE2EA` | Bordes y divisores |
| text | `#202733` | Texto principal |
| muted | `#687385` | Texto secundario, iconos inactivos |
| accent | `#3278D9` | Acción primaria, foco |
| hover | `#E8F1FF` | Hover de controles |
| selection | `#D5E6FF` | Selección |

Colores expuestos como `uint32_t` ARGB opaco (`MDE_COLOR_RGB`).

## Contrato de API

```c
mde_theme_id theme_get_default(void);           /* siempre MDE_LIGHT */
const mde_theme *theme_get(mde_theme_id id);
const mde_palette *theme_palette(mde_theme_id id);
```

`MDE_DARK` está reservado; si se solicita sin implementar, `theme_get` devuelve Light (arranque seguro).

## Controles

- **Barra:** altura 28 px, fondo `surface`, borde inferior `border`.
- **Botón:** 28 px alto, radio 6 px, variante estándar y primaria (`mde_button_style_*`).
- **Ventana:** radio 10 px, título 28 px, sin estética de “card” web.

## Integración pendiente (otras misiones)

- SA-03: añadir `mde/theme/light.c` y `mde/theme/theme.c` al Makefile.
- Shell MDE / compositor: incluir `controls.h` y pintar con `theme_palette(theme_get_default())`.

## Pruebas

```bash
# WSL / Linux con gcc
gcc -std=c11 -Wall -Wextra -c mde/theme/light.c -o /tmp/light.o
gcc -std=c11 -Wall -Wextra -c mde/theme/theme.c -o /tmp/theme.o
```

Verificación lógica: `theme_get_default() == MDE_LIGHT` y colores de `mde_light_palette` coinciden con la tabla.

## Criterios de aceptación

- [x] Paleta exacta en `light.h` / `light.c`
- [x] Tema claro predeterminado en `theme.c`
- [x] Estilos botón, barra y ventana en `controls.h`
- [x] Sin modificaciones en kernel
- [x] Reporte SA-18 en `docs/swarm/`
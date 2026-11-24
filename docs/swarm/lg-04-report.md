# LG-04 — Dock + Top Bar Liquid Glass

| Campo | Valor |
|-------|-------|
| Misión | LG-04 Shell chrome Liquid Glass (dock + topbar) |
| Agente | dock-topbar-engineer |
| Modelo | Composer 2.5 |
| Estado | COMPLETED (estilo glass, draw lists, flags material) |
| Fecha | 2026-11-22 |

## Objetivo

Migrar el chrome SA-20 a **Liquid Glass**: topbar 28 px en cápsula translúcida con hover pill en menús; dock en **píldora flotante** con radio alto, sombras glass, indicador activo azul glass, halo hover (~1.12×) y etiquetas **Notes / Files / Terminal**. Exponer `DOCK_MATERIAL_GLASS` y `TOPBAR_MATERIAL_GLASS` para el compositor (blur).

## Archivos tocados

| Archivo | Rol |
|---------|-----|
| `mde/shell/dock.h` | Material glass, estilo extendido, hover, `dock_get_material()` |
| `mde/shell/dock.c` | Tokens `mde_light_glass`, píldora 20 px, sombras 3 capas |
| `mde/shell/topbar.h` | API pública simétrica a `dock.h`, estilo glass, hover menú |
| `mde/shell/topbar.c` | Cápsula con margen 12 px, pills menú, reloj sin calendario |
| `docs/swarm/lg-04-report.md` | Este informe |

**Fuera de alcance (respetado):** `wallpaper.c`, compositor, `compositor_shell.h`.

## Flags y material (compositor)

| Símbolo | Valor | Getter |
|---------|-------|--------|
| `DOCK_MATERIAL_GLASS` | `1` | `dock_get_material()` |
| `TOPBAR_MATERIAL_GLASS` | `1` | `topbar_get_material()` |

Blur sugerido vía `dock_get_style()->blur_radius` (`MDE_GLASS_BLUR_HEAVY`) y `topbar_get_glass_style()->blur_radius` (`MDE_GLASS_BLUR_LIGHT`).

## Dock — Liquid Glass

| Aspecto | Token / valor |
|---------|----------------|
| Material | `DOCK_MATERIAL_GLASS` |
| Fondo | `MDE_GLASS_FILL_PRIMARY` (~90 % blanco) |
| Radio | `MDE_CTRL_GLASS_DOCK_RADIUS` (20 px) |
| Margen inferior | `MDE_CTRL_DOCK_MARGIN_BOTTOM` (≥ 12 px) |
| Sombras | 3 capas `mde_light_glass.shadow[]` |
| Borde | specular superior + `border_base` inferior |
| Indicador activo | `MDE_GLASS_ACCENT` 8×5 px, alpha glass |
| Hover | `dock_set_hovered_index()` — halo chrome ~1.12×; ícono 40×40 (contrato compositor) |

### Ítems por defecto (`dock_init`)

| Índice | `app_id` | Etiqueta |
|--------|----------|----------|
| 0 | `mde.notes` | Notes |
| 1 | `mde.files` | Files |
| 2 | `mde.terminal` | Terminal |

### `dock_rect_cmd.kind`

| kind | Significado |
|------|-------------|
| 0 | Panel / sombra |
| 1 | Borde |
| 2 | Indicador activo azul glass |
| 3 | Franja specular |
| 4 | Ícono / halo hover |

## Topbar — Liquid Glass

| Aspecto | Valor |
|---------|--------|
| Altura | `MDE_CTRL_GLASS_TOPBAR_HEIGHT` = **28 px** |
| Cápsula | Ancho viewport − **24 px** (margen 12 px lateral) |
| Fondo | `MDE_GLASS_FILL_SECONDARY` |
| Menús | Hover pill (`kind` 4) con `MDE_GLASS_FILL_CHROME` |
| Zonas | Logo, app activa, menús izq.; reloj **HH:MM** + **Cfg** der. |
| Calendario | **`MDE_TOPBAR_SHOW_CALENDAR` = 0** (`#error` si se activa) |

### API principal

```c
void dock_init(void);
uint32_t    dock_item_count(void);
const char *dock_item_label(uint32_t index);
uint8_t     dock_get_material(void);

void topbar_init(void);
uint8_t                    topbar_get_material(void);
const topbar_glass_style_t *topbar_get_glass_style(void);
void topbar_set_hovered_menu(int32_t menu_index);
```

## Dependencias

- **LG / glass:** `mde/theme/glass.h`, `mde_glass_dock_style_light`, `mde_bar_glass_style_light`.
- **SA-15:** `comp_paint_dock_cmds` (alpha en dock); topbar aún sin alpha en compositor (siguiente paso).
- **SA-20:** Contrato `dock_item_count` / etiquetas bajo íconos 40×40.

## Pruebas

```bash
gcc -std=c11 -Wall -Wextra -Imde -c mde/theme/glass.c -o /tmp/glass.o
gcc -std=c11 -Wall -Wextra -Imde -c mde/theme/theme.c -o /tmp/theme.o
gcc -std=c11 -Wall -Wextra -Imde -c mde/shell/dock.c -o /tmp/dock.o
gcc -std=c11 -Wall -Wextra -Imde -c mde/shell/topbar.c -o /tmp/topbar.o
```

## Criterios de aceptación

- [x] Topbar 28 px glass, cápsula, hover pill menús, sin calendario
- [x] Dock píldora flotante, margen ≥ 12 px, radio 20, indicador azul glass
- [x] Hover dock (halo scale) + `dock_set_hovered_index`
- [x] Labels Notes / Files / Terminal
- [x] `DOCK_MATERIAL_GLASS` / `TOPBAR_MATERIAL_GLASS` + getters
- [x] `dock_init`, `dock_item_count`, `dock_item_label`, `topbar_init` preservados
- [x] `topbar.h` público
- [x] Sin `wallpaper.c`

## Próximos pasos

1. Compositor: blur backdrop usando `material` + `blur_radius`; alpha en `topbar_rect_cmd`.
2. Enlazar `mde_interaction_icon_hover_scale` → `dock_set_hovered_index`.
3. `mde_interaction` → `topbar_set_hovered_menu` para pills.
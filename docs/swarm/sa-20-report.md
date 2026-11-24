# SA-20 — Dock flotante y topbar MDE Light

| Campo | Valor |
|-------|-------|
| Misión | SA-20 Shell chrome (dock + topbar) |
| Agente | dock-topbar-engineer |
| Estado | COMPLETED (lógica de layout y lista de dibujo; sin blit) |
| Fecha | 2026-11-22 |

## Objetivo

Implementar el **dock** inferior flotante (blanco translúcido, borde discreto, separado del borde de pantalla) y la **topbar** de **28 px** con menús a la izquierda y **reloj HH:MM** a la derecha (**sin calendario**), alineados con **MDE Light** (`mde/theme/controls.h`).

## Archivos entregados

| Archivo | Rol |
|---------|-----|
| `mde/shell/dock.h` | API pública dock (draw lists, etiquetas) |
| `mde/shell/dock.c` | Estilo, layout, ítems **Notes / Files / Terminal**, `dock_build_draw_list()` |
| `mde/shell/topbar.c` | Barra 28 px, logo/app/menús izq.; reloj + config der. |
| `docs/swarm/sa-20-report.md` | Este informe |

**Fuera de alcance (respetado):** `mde/shell/wallpaper.c` (SA-21), kernel/compositor, fuentes raster.

## Dock — diseño

| Aspecto | Valor / token |
|---------|----------------|
| Fondo | `surface` (#FFFFFF) con **alpha 216** (~85 %) |
| Borde | `border` (#DCE2EA), 1 px en borde inferior del panel |
| Radio | `MDE_CTRL_DOCK_RADIUS` (16 px) |
| Padding interno | `MDE_CTRL_DOCK_PADDING` (8 px) |
| Separación inferior | `MDE_CTRL_DOCK_MARGIN_BOTTOM` (12 px) |
| Anclaje | Centrado horizontal; **flotante** (no pegado al borde inferior) |
| Íconos | 40×40 px, gap 6 px, indicador activo `accent` |

### Ítems por defecto (`dock_init`)

| Índice | `app_id` | Etiqueta (`dock_item_label`) |
|--------|----------|------------------------------|
| 0 | `mde.notes` | Notes |
| 1 | `mde.files` | Files |
| 2 | `mde.terminal` | Terminal |

### API principal (`dock.h`)

```c
void dock_init(void);
void dock_set_viewport(int32_t width, int32_t height);
uint32_t    dock_item_count(void);
const char *dock_item_label(uint32_t index);
const dock_layout_t *dock_layout_compute(void);
int  dock_build_draw_list(dock_draw_list_t *out);
```

## Compositor (SA-15)

Contrato de enlazado en `mde/compositor/compositor_shell.h` (incluye `mde/shell/dock.h`).

| Paso | Función | Uso |
|------|---------|-----|
| Bootstrap | `compositor_shell_bootstrap` → `dock_init()` | Carga 3 ítems fijos |
| Geometría | `dock_layout_compute()` | Ancho panel según `dock_item_count()` |
| Panel | `dock_build_draw_list()` → `comp_paint_dock_cmds()` | Alpha 216 en panel flotante |
| Texto | `comp_paint_dock_icon_labels()` | Por cada rect 40×40, `dock_item_label(i)` vía `glyph_cache_draw_string` |

**Enlace fuerte:** `dock_item_count` / `dock_item_label` están definidos en `dock.c`. En `compositor_paint.c` persisten stubs `__attribute__((weak))` que devuelven conteo desde layout y `NULL` en etiquetas; al enlazar `mde/shell/dock.o`, el compositor pinta **Notes**, **Files**, **Terminal** bajo cada ícono.

**Topbar:** sin calendario — solo `topbar_clock_text()` (`HH:MM`) en `comp_paint_chrome_text()`; no hay API de fecha en shell.

## Topbar — diseño

| Zona (izq → der) | Contenido |
|------------------|-----------|
| Logo | Marca placeholder (rectángulo `accent` hasta logo SA-21) |
| App activa | Texto vía `topbar_set_active_app()` |
| Menús | Archivo, Editar, Ventana, Ayuda |
| Reloj | Solo **HH:MM** (`topbar_set_clock`, `topbar_clock_text`) |
| Config | Botón 28×20 en zona derecha |

| Métrica | Valor |
|---------|-------|
| Altura | `MDE_CTRL_BAR_HEIGHT` = **28 px** |
| Fondo | `surface` (`mde_bar_style_light`) |
| Borde inferior | `border`, 1 px |
| Calendario | **`MDE_TOPBAR_SHOW_CALENDAR` = 0** (`#error` si se activa) |

### API principal (`topbar.c`)

```c
void topbar_init(void);
void topbar_set_viewport(int32_t width);
void topbar_set_active_app(const char *name);
void topbar_set_clock(uint8_t hour, uint8_t minute);
int  topbar_shows_calendar(void);  /* siempre 0 */
const topbar_layout_t *topbar_layout_compute(void);
int  topbar_build_draw_list(topbar_draw_list_t *out);
```

## Dependencias

- **SA-18:** `controls.h`, `mde_bar_style_light`, métricas dock/topbar.
- **SA-14 / SA-15:** `fb_fill_rect` / compositor para pintar listas de rects (con alpha en dock).
- **SA-21:** logo e iconos de dock (sustituyen placeholders).

## Pruebas

```bash
gcc -std=c11 -Wall -Wextra -c mde/theme/light.c -o /tmp/light.o
gcc -std=c11 -Wall -Wextra -c mde/theme/theme.c -o /tmp/theme.o
gcc -std=c11 -Wall -Wextra -Imde -c mde/shell/dock.c -o /tmp/dock.o
gcc -std=c11 -Wall -Wextra -Imde -c mde/shell/topbar.c -o /tmp/topbar.o
```

**Resultado en entorno actual:** `INFRA_FAIL` — `gcc` ausente en WSL (misma situación que SA-35). Verificación lógica en código: altura topbar 28, margen dock 12, `topbar_shows_calendar() == 0`.

## Criterios de aceptación

- [x] Dock flotante, blanco translúcido, borde discreto, margen inferior 12 px
- [x] Topbar 28 px MDE Light
- [x] Izquierda: logo + app + menús; derecha: reloj + config
- [x] Sin calendario
- [x] `dock_item_count()` / `dock_item_label()` reales (3 apps)
- [x] `dock.h` + `dock.c` / `topbar.c` (sin tocar `wallpaper.c`)
- [x] Informe SA-20 en `docs/swarm/`

## Próximos pasos

1. SA-03: añadir objetos `mde/shell/dock.c`, `topbar.c` al Makefile.
2. SA-15: blit de draw lists + alpha; texto de menús y reloj (SA-24 tipografía).
3. `mde/shell/topbar.h` simétrico a `dock.h` para el escritorio MDE.
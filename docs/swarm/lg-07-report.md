# LG-07 — Wallpaper + splash Liquid Glass

| Campo | Valor |
|-------|-------|
| Misión | LG-07 Wallpaper, bootsplash e iconos dock |
| Agente | wallpaper-brand-engineer |
| Estado | COMPLETED |
| Fecha | 2026-11-22 |

## Objetivo

Recursos visuales auténticos para **Liquid Glass**: fondo luminoso que aporte color al blur frosted, iconografía RGBA del dock y bootsplash claro con barra de progreso, **sin** sustituir el logo metálico oficial.

## Entregables

| Archivo | Descripción |
|---------|-------------|
| `assets/wallpaper/light_default.{png,h}` | Paisaje abstracto 192×108 perla/azul celeste (RGBA8888) |
| `assets/icons/icons_mde.h` | Files / Notes / Terminal en 40×40 y 32×32 |
| `assets/icons/{files,notes,terminal}.png` | Fuentes PNG del dock y launcher |
| `assets/tools/generate_lg07_assets.py` | Regeneración determinista PNG + headers |
| `mde/shell/wallpaper.c` | Carga + **muestreo bilineal** entero (sin `float`) |
| `kernel/bootsplash.c` | Pintado framebuffer, progreso, contrato SA-21 |
| `assets/README.md` | Estado actualizado del pipeline |

## Wallpaper

- Paleta: blanco perla `#F6F8FB` con manchas suaves `#6AB8F0`–`#C5E6FF` y streak specular para que el glass muestre tinte azul bajo blur (`MDE_GLASS_BLUR_*`).
- Resolución fuente **192×108** (antes 128×72).
- API añadida en `wallpaper.c`:
  - `wallpaper_sample_bilinear(img, x, y, dst_w, dst_h)` — RGBA8888
  - `wallpaper_blit_bilinear` — salida `0x00RRGGBB` para framebuffer
  - `wallpaper_blit_bilinear_rgba` — salida RGBA para superficies del compositor

## Iconos dock

| `app_id` | Archivo | Notas |
|----------|---------|-------|
| `mde.notes` | `notes.png` | Hoja con líneas finas |
| `mde.files` | `files.png` | Carpeta azul perla |
| `mde.terminal` | `terminal.png` | Consola clara + prompt geométrico |

Tabla `mde_dock_icons[]` en `icons_mde.h` alinea con el orden del dock (Notes, Files, Terminal).

## Bootsplash

- Implementa `bootsplash_on_mde_ready`, `bootsplash_phase`, `bootsplash_should_hold`, `bootsplash_release` (`kernel/include/bootsplash.h`).
- Fondo en gradiente claro, **slot reservado** 56×56 para logo oficial (marco `#DCE2EA`), texto bitmap `mettaos` neutro.
- Barra de progreso: pista `#DCE2EA`, relleno `#3278D9`; `bootsplash_set_progress(0–1000)`.
- Helpers para cableado futuro en `kmain`: `bootsplash_early_init`, `bootsplash_paint`, `bootsplash_tick`.
- Retención post-MDE: `BOOTSPLASH_POST_MDE_HOLD_MS` (**2000 ms**).

### Logo metálico

**No existe** el activo canónico «M» en `assets/logo/`. El splash usa slot + tipografía neutra; **no** se inventó logotipo sustituto.

## Integración pendiente (fuera de LG-07)

| Consumidor | Acción |
|------------|--------|
| `kernel/kernel.c` | Invocar `bootsplash_early_init` tras `fb_init` y `bootsplash_tick` en el loop |
| Compositor LG-03 | Pintar wallpaper con `wallpaper_blit_bilinear*` antes del chrome glass |
| Dock / compositor | Blit de `mde_dock_icons[]` en celdas 40×40 |

## Verificación

```bash
# Regenerar assets
python assets/tools/generate_lg07_assets.py

# Compilar módulos LG-07 (freestanding, sin SSE)
gcc ... -c mde/shell/wallpaper.c
gcc ... -c kernel/bootsplash.c
```

## Dependencias

- SA-14 `fb.h` para `bootsplash_paint`.
- LG-03 compositor para blur + wallpaper en escritorio.
- Logo oficial en `assets/logo/` cuando diseño lo entregue.
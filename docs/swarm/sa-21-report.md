# SA-21 — Branding & Assets (informe)

| Campo | Valor |
|-------|-------|
| Misión | SA-21 Branding/assets |
| Agente | wallpaper-brand-engineer |
| Estado | COMPLETED (artefactos iniciales) |
| Fecha | 2026-11-22 |

## Entregables

| Archivo | Descripción |
|---------|-------------|
| `assets/README.md` | Pipeline de recursos, convenciones RGBA, aviso logo oficial ausente |
| `assets/wallpaper/light_default.png` | Placeholder visual (128×72, gradiente claro minimalista) |
| `assets/wallpaper/light_default.h` | Array `RGBA8888` embebible |
| `assets/icons/` | Directorio reservado (vacío) |
| `assets/logo/` | Directorio reservado — **sin logo metálico oficial** |
| `mde/shell/wallpaper.c` | Stub: carga wallpaper por defecto desde header embebido |
| `kernel/include/bootsplash.h` | Contrato retención splash **2000 ms** tras MDE listo |

## Fuera de alcance (explícito)

- Implementación de dibujo bootsplash en kernel (`bootsplash.c`, framebuffer).
- Símbolo «M» metálico u otro logo sustituto.
- Iconos de aplicaciones y integración compositor (depende SA-14 / MDE shell).
- Script `embed_rgba.py` en Makefile (SA-03).

## Contrato bootsplash

- Constante: `BOOTSPLASH_POST_MDE_HOLD_MS` = **2000**.
- Secuencia: `bootsplash_on_mde_ready()` → fase `HOLD` durante 2 s → `bootsplash_release()` / `should_hold()` en falso.
- Las funciones se declaran en el header; cuerpos pendientes de misión de gráficos/diagnóstico.

## Wallpaper

- Tema: MDE Light, fondo claro sin marca.
- API stub en `wallpaper.c`: `wallpaper_load_default()`, `wallpaper_get_active()`, `wallpaper_unload()`.
- Sin escalado ni blit (integración futura con framebuffer/compositor).

## Riesgos / dependencias

| Dependencia | Nota |
|-------------|------|
| Logo oficial | Bloquea splash con marca y dock branding final |
| SA-03 build | Incluir rutas `-I` para `assets/` y `kernel/include/` |
| SA-14 framebuffer | Necesario para pintar wallpaper en pantalla |

## Verificación sugerida (posterior)

1. Compilar `wallpaper.c` con `-I` raíz del repo.
2. Confirmar que `light_default.png` y primeras filas del `.h` coinciden visualmente.
3. Tests de contrato bootsplash (mock tiempo monotónico) cuando exista `bootsplash.c`.

## Próximos pasos

1. Incorporar logo metálico oficial en `assets/logo/` sin alterar el placeholder de wallpaper.
2. Añadir `mde/shell/wallpaper.h` y cablear al ciclo de vida del escritorio.
3. Implementar `bootsplash.c` respetando `BOOTSPLASH_POST_MDE_HOLD_MS`.
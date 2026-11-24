# MettaOS — pipeline de recursos (SA-21)

Árbol previsto:

| Ruta | Contenido |
|------|-----------|
| `wallpaper/` | Fondos de escritorio embebibles (PNG fuente + `.h` RGBA) |
| `icons/` | Iconos de aplicaciones y shell (PNG/SVG fuente → rasterizado) |
| `logo/` | **Logo metálico oficial «M»** y variantes aprobadas |

## Flujo de trabajo

1. **Autoría** — Diseño en herramienta externa; export PNG (sRGB, sin perfil exótico).
2. **Validación** — Revisión MDE Light (contraste, legibilidad sobre fondo claro).
3. **Embeddado** — Generar o actualizar el header C con array `RGBA8888` (véase `wallpaper/light_default.h`).
4. **Integración** — `mde/shell/wallpaper.c` referencia el header; el compositor escala al framebuffer (futuro).
5. **Registro** — Documentar en `docs/swarm/sa-21-report.md` y en el log de ejecución del enjambre.

Script de regeneración (manual, hasta toolchain SA-03):

```text
# Ejemplo: re-exportar wallpaper desde PNG canónico
python scripts/embed_rgba.py assets/wallpaper/light_default.png \
  --out assets/wallpaper/light_default.h
```

*(El script `embed_rgba.py` se añadirá con el sistema de build; por ahora la cabecera va versionada junto al PNG.)*

## Estado actual

| Recurso | Estado |
|---------|--------|
| `wallpaper/light_default.{png,h}` | Wallpaper Tahoe LG-T06 (192×108, perla/cielo azul) |
| `icons/icons_mde.h` + PNG dock | Notes/Files/Terminal/Calculator 40×40 y 32×32 (ver prompts en `assets/tools/generate_lg07_assets.py`) |
| `icons/` | Ver `icons_mde.h`; regenerar con `assets/tools/generate_lg07_assets.py` |
| `logo/` | **Falta el logo metálico oficial «M»** |

### Logo metálico oficial

**No existe en el repositorio el activo canónico del símbolo «M» metálico de MettaOS.**

- No inventar ni sustituir por otro logotipo en código, splash o marca.
- Cuando el asset oficial se entregue, colocarlo solo en `assets/logo/` con nombre acordado por diseño (p. ej. `metta_m_metallic.png`) y documentar la variante para fondos blancos con transparencia.
- Hasta entonces, bootsplash y shell deben omitir marca gráfica o usar texto neutro acordado con producto.

## Convenciones técnicas

- Formato embebido por defecto: **RGBA8888** (`0xRRGGBBAA`), filas arriba→abajo, columnas izquierda→derecha.
- Constantes de tamaño: `WALLPAPER_*_WIDTH` / `HEIGHT` en el `.h` correspondiente.
- Los PNG en git son la **fuente visual**; el `.h` es lo que compila el kernel/MDE en entornos freestanding.
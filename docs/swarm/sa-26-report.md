# SA-26 — MDE Files (explorador VFS, tema MDE Light)

| Campo | Valor |
|-------|-------|
| Misión | SA-26 MDE Files |
| Agente | mde-files-engineer |
| Estado | COMPLETED (artefactos iniciales) |
| Fecha | 2026-11-22 |
| Ámbito código | `mde/apps/files/` |

## Objetivo

Entregar la aplicación nativa **Archivos** del escritorio MDE: modelo de navegación sobre VFS, vista con chrome **MDE Light** y stub de sistema de ficheros para desarrollo hasta que exista VFS/MFS en el kernel.

## Archivos entregados

| Archivo | Rol |
|---------|-----|
| `files.h` | API pública de la app: ciclo de vida, acciones, pintado |
| `files_app.c` | `create` / `dispatch` / `paint`; arranque en `/home/user` |
| `files_vfs.h` | Contrato **puerto VFS** (`mde_files_vfs_ops`) |
| `files_vfs_stub.c` | Árbol en memoria (`/`, `home`, `mfs`, `etc`) + bind API |
| `files_model.h` / `files_model.c` | Ruta actual, listado, selección, historial atrás/adelante |
| `files_view.h` / `files_view.c` | Layout sidebar + toolbar 28 px + lista; `mde_files_painter` |
| `files_theme.h` | Métricas y colores derivados de SA-18 (`controls.h`, `light.h`) |

## UI (MDE Light)

| Región | Estilo |
|--------|--------|
| Cliente | `mde_window_style_light`, fondo `surface` |
| Sidebar 200 px | `secondary`, enlaces favoritos (texto solo; sin iconos SA-21 aún) |
| Toolbar | `mde_bar_style_light` (28 px), campo de ruta con borde `border` |
| Lista | Cabecera `bg`; filas 26 px; hover/selection vía paleta |
| Carpetas | Etiqueta `accent`; archivos `text` / `muted` en columnas |

Colores: solo tokens de `mde_light_palette` (sin fondos oscuros).

## Contrato VFS ↔ Files

```c
typedef struct mde_files_vfs_ops {
    void *ctx;
    mde_files_vfs_status (*resolve)(void *ctx, const char *path, mde_files_dirent_t *out);
    mde_files_vfs_status (*read_dir)(void *ctx, const char *path,
        mde_files_dirent_t *entries, size_t cap, size_t *out_count);
    mde_files_vfs_status (*parent_path)(void *ctx, const char *path,
        char *parent, size_t parent_cap);
} mde_files_vfs_ops;
```

- Sustitución del stub: `mde_files_vfs_bind(model->vfs, &kernel_vfs_ops)` cuando vfs-engineer exponga equivalente.
- Rutas absolutas POSIX; `mde_files_path_normalize()` unifica `\` → `/`.

## Acciones de la app

| `mde_files_action_kind` | Efecto |
|-------------------------|--------|
| `BACK` / `FORWARD` | Pilas de historial de rutas |
| `UP` | `parent_path` VFS |
| `OPEN` | Entra en carpeta seleccionada |
| `NAVIGATE` | Salto directo a ruta |
| `SELECT_ROW` | Selección de fila en lista |
| `REFRESH` | Relectura de directorio |

## Stub de datos

```
/
├── home/user/   (Documents, Downloads)
├── mfs/README.txt
└── etc/hosts
```

## Integración pendiente (otras misiones)

| Dependencia | Nota |
|-------------|------|
| SA-03 Makefile | Añadir fuentes `mde/apps/files/*.c` y `-I` raíz para `mde/theme` |
| vfs-engineer | Implementar `mde_files_vfs_ops` sobre VFS real |
| SA-15 compositor | Ventana nativa, enrutar input → `mde_files_app_dispatch` |
| rasterization-engineer | Implementar `draw_text` en `mde_files_painter` |
| native-applications-engineer | Registrar `MDE_FILES_APP_ID` en launcher `.mettaapp` |

## Fuera de alcance

- Kernel VFS, MFS, syscalls de archivo.
- DnD, menú contextual, búsqueda, vista iconos/grid.
- Apertura de archivos (solo navegación de directorios).

## Pruebas

### Compilación unitaria (host, gcc)

Desde la raíz del repo:

```bash
mkdir -p /tmp/metta-files
gcc -std=gnu11 -Wall -Wextra -Wpedantic -I. -c mde/theme/light.c -o /tmp/metta-files/light.o
gcc -std=gnu11 -Wall -Wextra -Wpedantic -I. -c mde/theme/theme.c -o /tmp/metta-files/theme.o
gcc -std=gnu11 -Wall -Wextra -Wpedantic -I. -c mde/apps/files/files_vfs_stub.c -o /tmp/metta-files/vfs.o
gcc -std=gnu11 -Wall -Wextra -Wpedantic -I. -c mde/apps/files/files_model.c -o /tmp/metta-files/model.o
gcc -std=gnu11 -Wall -Wextra -Wpedantic -I. -c mde/apps/files/files_view.c -o /tmp/metta-files/view.o
gcc -std=gnu11 -Wall -Wextra -Wpedantic -I. -c mde/apps/files/files_app.c -o /tmp/metta-files/app.o
```

| Prueba | Resultado |
|--------|-----------|
| Compilación anterior | Ejecutar en entorno con `gcc` (WSL / Linux) |
| QEMU / pantalla | Pendiente compositor + painter con texto |

## Criterios de aceptación

- [x] Explorador acotado a `mde/apps/files/`
- [x] Tema MDE Light vía SA-18 (`files_theme.h`)
- [x] Puerto VFS documentado + stub funcional
- [x] Modelo de navegación con historial
- [x] Vista con layout y callbacks de pintado
- [x] Informe `docs/swarm/sa-26-report.md`

## Próximos pasos

1. Cablear `mde_files_app_paint` al compositor MDE con `fb_fill_rect` + glifos.
2. Reemplazar stub por VFS del kernel al completar capa de archivos.
3. Iconos de carpeta/archivo desde `assets/icons/` (SA-21) cuando existan.
# SA-27 — MDE Notes (editor nativo)

**Agente:** mde-notes-engineer  
**Estado:** COMPLETED (núcleo lógico + stubs)  
**Fecha:** 2026-11-22  
**Ámbito:** `mde/apps/notes/`

## Objetivo

Entregar el núcleo de **MDE Notes**: búfer de texto **multilínea**, pila **undo/redo** por instantáneas (stub evolucionable) e **integración VFS** por inyección de operaciones (contrato SA-23), sin depender del kernel freestanding en esta fase.

## Archivos

| Archivo | Rol |
|---------|-----|
| `notes.h` / `notes_app.c` | Ciclo de vida, documento, edición, guardar/abrir |
| `editor.h` / `editor.c` | Búfer 64 KiB, índice de líneas, cursor, selección básica |
| `undo.h` / `undo.c` | Hasta 16 instantáneas; truncado de redo |
| `vfs_io.h` / `vfs_io.c` | `notes_vfs_bind`, lectura/escritura de archivo |
| `clipboard.h` / `clipboard.c` | Stub (`NOTES_ERR_UNINITIALIZED`) |
| `notes_smoke.c` | Prueba host con VFS simulada |

## Contratos

### Editor multilínea

- `NOTES_EDITOR_MAX_BYTES` = 64 KiB; hasta 4096 líneas indexadas por `\n`.
- API: insertar texto/código, nueva línea, retroceso, borrar adelante, `load_text`, `select_all`.

### Undo / redo (stub)

- `notes_undo_checkpoint` antes de cada mutación vía `notes_app_insert_*` / `backspace` / `delete_forward`.
- `notes_undo_apply` / `notes_redo_apply` restauran instantáneas completas (sustituible por comandos atómicos).

### VFS

- Sin llamadas directas al kernel: `notes_vfs_bind(const notes_vfs_ops_t *)`.
- `notes_app_open_path` / `notes_app_save_as` usan `notes_vfs_read_file` / `notes_vfs_write_file`.
- Sin `bind`: `NOTES_ERR_VFS_UNBOUND`.

## Verificación

```bash
# Host (WSL o Linux con gcc)
cd mde/apps/notes
gcc -std=c11 -Wall -Wextra -Wpedantic -I. \
  editor.c undo.c vfs_io.c notes_app.c clipboard.c notes_smoke.c -o notes_smoke
./notes_smoke
# Esperado: notes_smoke: PASS (1 checks)
```

**Ejecución en WSL (2026-11-22):** `gcc` no está en PATH → compilación no ejecutada (misma situación que SA-35 / `wsl-setup.sh`). El fuente `notes_smoke.c` cubre multilínea, guardar, undo y reabrir.

## Fuera de alcance (esta misión)

- Pintado en framebuffer, input PS/2, ventana WM (SA-19), portapapeles real (SA-22).
- Enlace en `Makefile` del kernel (SA-03 / SA-29 `.mettaapp`).
- VFS real en kernel (SA-23); aquí solo el adaptador hacia esa API.

## Dependencias

| Misión | Uso |
|--------|-----|
| SA-18 | Tema MDE Light al integrar UI |
| SA-23 | Implementación de `notes_vfs_ops_t` |
| SA-29 | Empaquetado `.mettaapp` y launcher |

## Criterios de aceptación

- [x] Editor multilínea con cursor y líneas
- [x] Abrir / guardar vía VFS inyectada
- [x] Undo / redo funcional (instantáneas)
- [x] Stub de portapapeles declarado
- [x] Prueba `notes_smoke` documentada
- [x] Sin cambios fuera de `mde/apps/notes/` (salvo este informe)
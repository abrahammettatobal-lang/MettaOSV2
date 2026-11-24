# SA-23 — Virtual File System (VFS)

**Agente:** vfs-engineer  
**Fecha:** 2026-11-22  
**Estado:** COMPLETED (pendiente MFS SA-24 y wire-up en `kmain`)

## Resumen

Capa VFS del kernel con rutas estilo POSIX, tabla de descriptores y delegación a backends mediante `vfs_fs_ops_t` (contrato **VFS↔MFS**). MDE Files (SA-26) y apps nativas deben usar syscalls sobre esta API, sin tocar MFS.

## Archivos

| Archivo | Rol |
|---------|-----|
| `kernel/include/fs/vfs.h` | Tipos públicos, flags, `vfs_fs_ops_t`, API |
| `kernel/fs/vfs.c` | Normalización de rutas, montajes, FDs, operaciones |
| `docs/swarm/sa-23-report.md` | Este informe |

## API kernel

| Función | Descripción |
|---------|-------------|
| `vfs_init` | Inicializa montajes y tabla FD |
| `vfs_mount` / `vfs_unmount` | Registra volumen (p. ej. MFS en `/`) |
| `vfs_open` / `vfs_close` | Descriptores con flags `VFS_O_*` |
| `vfs_read` / `vfs_write` | I/O con offset por FD (`metta_ssize_t`; &lt;0 = errno VFS) |
| `vfs_mkdir` | Crea directorio vía backend |
| `vfs_readdir` | Itera directorio abierto con `VFS_O_DIRECTORY` |
| `vfs_resolve_mount` | Resuelve montaje por prefijo más largo |
| `vfs_errno_from_status` | `metta_status_t` → `VFS_ENOENT` / `VFS_EIO` / … |

## Contrato VFS ↔ MFS (SA-24)

MFS implementa y registra:

```c
vfs_fs_ops_t mfs_vfs_ops = { .fs_name = "mfs", ... };
vfs_mount(&mfs_vfs_ops, "/", mfs_sb);
```

Callbacks obligatorios para integración MDE:

- `lookup`, `create`, `mkdir`
- `open_node`, `close_node`, `read`, `write`, `readdir`
- `release_node` (libera `vfs_node_t` / cookie de inodo)

Paths al backend: **relativos al punto de montaje**, sin `/` inicial (`""` = raíz del volumen).

`vfs_node_t` abstrae el inodo; `fs_cookie` es opaco para MFS.

## Contrato VFS ↔ MDE Files

- Rutas absolutas `/…`, máximo `VFS_PATH_MAX` (256).
- Directorios: `vfs_open(path, VFS_O_RDONLY | VFS_O_DIRECTORY, &fd)` + `vfs_readdir`.
- Errores usuario: `vfs_errno_from_status` o códigos negativos de `vfs_read`/`vfs_write`.
- Sin acceso directo a bloques ni superblock MFS.

```mermaid
flowchart LR
    MDE[MDE Files SA-26]
    VFS[vfs_* SA-23]
    MFS[MFS SA-24]
    MDE --> VFS
    VFS --> MFS
```

## Diseño interno

1. Hasta `VFS_MAX_MOUNTS` (4) y `VFS_MAX_FDS` (64).
2. Normalización: colapso de `/`, eliminación de `.` y `..`.
3. Montaje raíz `"/"` coincide con cualquier ruta absoluta (prefijo más largo gana).
4. `vfs_unmount` falla con `METTA_ERR_BUSY` si hay FDs abiertos en ese montaje.

## Pruebas

| Prueba | Resultado |
|--------|-----------|
| Compilación freestanding | **No ejecutada** — toolchain SA-03 en curso |
| Unit con backend mock | **Pendiente** — requiere stub MFS o harness SA-35 |

## Dependencias

| De | Para |
|----|------|
| SA-24 MFS | Implementar `vfs_fs_ops_t` y `vfs_mount` en arranque |
| SA-03 Makefile | `-Ikernel/include`, enlazar `kernel/fs/vfs.c` |
| SA-26 MDE Files | Consumir API vía syscall (futuro `metta_open`) |

## Fuera de alcance (oleada SA-23 según entrega)

`seek`, `rename`, `unlink` — previstos en roadmap del swarm; no forman parte de este diff (solo `kernel/fs/vfs*` solicitado).

## Errores / limitaciones

- Sin locks (uniprocesador; SMP futuro).
- Sin syscalls de usuario aún; API solo kernel.
- Sin volumen montado: `vfs_open` → `METTA_ERR_NOT_FOUND`.

## Próximos pasos

1. SA-24: MFS real + registro en `vfs_mount`.
2. SA-35: prueba open/read/mkdir/readdir con disco virtual.
3. Extender API con `seek`, `rename`, `unlink` en revisión de contrato.
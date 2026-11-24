# SA-24 — Metta File System (MFS)

| Campo | Valor |
|-------|-------|
| Misión | SA-24 — superblock, inodos, dirent, bloques persistentes |
| Agente | mfs-engineer |
| Estado | COMPLETED (MFS on-disk + VFS + `vd0`) |
| Fecha | 2026-11-22 |
| Alcance | Solo `kernel/fs/mfs/` |

## Resumen

**MFS v1** con layout 4 KiB, integridad CRC32, bitmaps y `dirent`. Persistencia sobre el disco virtual **`vd0`** (SA-25) mediante adaptador sectorial 512→4096. Integración **VFS (SA-23)** vía `mfs_vfs_ops` y montaje en `/` con `mfs_vfs_kmain_stub()`.

## Archivos

| Ruta | Rol |
|------|-----|
| `mfs_format.h` | Layout on-disk, superbloque, inodo, `dirent` |
| `mfs.h` / `mfs*.c` | Core: format, mount, bmap, inodo, directorio |
| `blkdev.h` / `blkdev_stub.c` | Backend RAM local (tests sin block layer) |
| `blkdev_vd.h` / `blkdev_vd.c` | Backend `metta_block_dev_t` (`block_read`/`block_write`) |
| `mfs_vfs.h` / `mfs_vfs.c` | `vfs_fs_ops_t`, montaje VFS, stub `kmain` |
| `mfs_internal.h`, `mfs_crc.c` | Internos e integridad |

## Layout on-disk (v1)

| Bloque LBA | Contenido |
|------------|-----------|
| 0 | Superblock (`MFS1`, CRC32) |
| 1 | Bitmap inodos |
| 2 | Bitmap bloques datos |
| 3 … | Tabla inodos (32 / bloque) |
| `data_start` … | Datos |

## VFS ↔ MFS

```c
#include "fs/mfs/mfs_vfs.h"   /* desde kmain: ruta según -I del árbol kernel */

/* Tras block_init, virt_disk_create_default + virt_disk_register, vfs_init: */
metta_status_t st = mfs_vfs_kmain_stub();  /* vd0 → MFS en "/" */
```

| Símbolo | Descripción |
|---------|-------------|
| `mfs_vfs_ops` | Tabla `vfs_fs_ops_t` (`lookup`, `create`, `mkdir`, I/O, `readdir`) |
| `mfs_vfs_mount_volume` | `metta_block_dev_t` + path; auto-`format` si volumen vacío/corrupto |
| `mfs_vfs_mount_vd0` | `block_lookup("vd0")` + montaje |
| `mfs_vfs_kmain_stub` | Monta `"/"` en `vd0` (punto de entrada `kmain`) |

Paths al backend: relativos al punto de montaje (`""` = raíz). `fs_cookie` del `vfs_node_t` = número de inodo.

## Disco virtual `vd0`

- **16 MiB** por defecto (SA-25) → **4096** bloques MFS de 4 KiB (`sector_count / 8`).
- Primer arranque: `mfs_mount` devuelve corrupto → `mfs_format` → segundo `mfs_mount`.
- Sustituye al stub `mfs_blkdev_stub` en producción; el stub sigue disponible para pruebas unitarias aisladas.

## Secuencia `kmain` (wire-up fuera de este ámbito)

```c
virt_disk_t vd;
block_init();
virt_disk_create_default(&vd);
virt_disk_register(&vd);
vfs_init();
(void)mfs_vfs_kmain_stub();
```

## API núcleo MFS (sin VFS)

Ver `mfs.h`: `mfs_format`, `mfs_mount`, `mfs_lookup`, `mfs_mkdir`, `mfs_create_file`, `mfs_read_file`, `mfs_write_file`, etc.

## Verificación

```bash
make print-objs   # kernel/fs/mfs/*.o
make -j
```

## Límites

- Sin `unlink`/`rename`; directorios solo bloques directos.
- Un volumen estático en `mfs_vfs.c` (suficiente para raíz `vd0`).
- `VFS_NAME_MAX` (64) acota nombres expuestos al VFS.

## Dependencias

| SA | Uso |
|----|-----|
| SA-23 | `vfs_mount`, tipos `vfs_fs_ops_t` |
| SA-25 | `vd0`, `block_lookup`, I/O 512 B |
| SA-03 | Compilación (`kernel/*/*/*.c`) |
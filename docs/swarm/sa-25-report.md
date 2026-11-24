# SA-25 — Driver de bloques y disco virtual (RAM / imagen)

| Campo | Valor |
|-------|-------|
| Misión | SA-25 Block storage |
| Agente | storage-driver-engineer |
| Estado | COMPLETED (capa kernel; integración `kmain` / VFS pendiente) |
| Fecha | 2026-11-22 |

## Resumen

Se añadió una capa de dispositivos de bloque de 512 bytes/LBA y un backend **disco virtual** con dos modos: RAM (buffer propio o externo) e **imagen lineal en memoria** (lectura o lectura-escritura). Las I/O copian sectores de forma síncrona en kernel freestanding, sin VirtIO ni ATA (misiones futuras).

## Archivos creados

| Archivo | Rol |
|---------|-----|
| `kernel/drivers/block/block.h` | API de registro, lookup y `block_read` / `block_write` |
| `kernel/drivers/block/block.c` | Tabla estática de hasta 8 dispositivos |
| `kernel/drivers/block/virt_disk.h` | Disco virtual RAM / imagen |
| `kernel/drivers/block/virt_disk.c` | I/O sectorial; RAM por defecto 16 MiB (`vd0`) |
| `docs/swarm/sa-25-report.md` | Este informe |

**Ámbito:** solo `kernel/drivers/block/` (sin cambios en `Makefile`).

## API pública

### Capa block

```c
metta_status_t block_init(void);
metta_status_t block_register(metta_block_dev_t *dev);
metta_status_t block_unregister(const char *name);
metta_block_dev_t *block_lookup(const char *name);
metta_status_t block_read(metta_block_dev_t *dev, metta_blk_lba_t lba,
                          void *buf, uint32_t count);
metta_status_t block_write(metta_block_dev_t *dev, metta_blk_lba_t lba,
                           const void *buf, uint32_t count);
```

- Sector lógico: **512** (`METTA_BLK_SECTOR_SIZE`).
- Errores: `metta_status_t` (`METTA_ERR_RANGE`, `METTA_ERR_PERM` en imagen solo lectura, etc.).
- Cada dispositivo implementa `read_sectors`; `write_sectors` opcional (solo lectura si es `NULL`).

### Disco virtual

```c
metta_status_t virt_disk_attach_ram(virt_disk_t *vd, const char *name,
                                    void *backing, uint64_t byte_length);
metta_status_t virt_disk_attach_image(virt_disk_t *vd, const char *name,
                                      void *image, uint64_t byte_length,
                                      bool image_writable);
metta_status_t virt_disk_create_default(virt_disk_t *vd); /* "vd0", 16 MiB estático */
metta_status_t virt_disk_register(virt_disk_t *vd);
void virt_disk_teardown(virt_disk_t *vd);
```

**Imagen en memoria:** puntero a blob ya cargado (p. ej. sección enlazada, carga PMM+VMM futura). No usa VFS ni archivos host en esta etapa.

## Secuencia sugerida en `kmain` (posterior)

```c
virt_disk_t root;
block_init();
virt_disk_create_default(&root);
virt_disk_register(&root);
/* MFS/VFS: block_lookup("vd0") */
```

## Compilación

El `Makefile` (SA-03) ya recoge `kernel/*/*/*.c`. Añadir include del driver:

```make
CFLAGS += -I$(ROOT)/kernel/drivers/block
```

Prueba unitaria freestanding:

```bash
gcc ... -Ikernel/include -Ikernel/drivers/block \
  -c kernel/drivers/block/block.c \
  -c kernel/drivers/block/virt_disk.c
```

## Pruebas ejecutadas

| Prueba | Resultado |
|--------|-----------|
| Compilación freestanding (`block.c`, `virt_disk.c`) | **OK** (WSL/gcc, flags kernel) |
| QEMU lectura/escritura sector | **Pendiente** — sin wire-up en `kmain` |
| Imagen solo lectura vs RAM RW | **Pendiente** — prueba manual en arranque |

## Errores / limitaciones

- Sin colas asíncronas, DMA ni IRQ de hardware.
- Disco por defecto: **16 MiB** estático en `.bss` (impacto en tamaño del kernel).
- Sin `virtio_blk.c` / ATA: fuera de alcance SA-25.
- Lookup por nombre es comparación byte a byte (sin hash).

## Dependencias

| De | Para |
|----|------|
| SA-03 Toolchain | `-Ikernel/drivers/block`, enlazar fuentes bajo `kernel/drivers/block/` |
| SA-01 `metta/types.h` | `metta_status_t` |
| SA futuro MFS/VFS | Consumir `block_read`/`block_write` sobre `vd0` o imagen |
| SA-08 VMM (opcional) | Buffer RAM grande fuera del enlace estático |

## Fuera de alcance

- VirtIO block, particionado GPT/MBR, cache de escritura.
- Lectura de imagen desde disco host (requiere VFS o payload Multiboot).

## Próximos pasos

1. Cablear `virt_disk_create_default` desde `kmain` tras PMM.
2. Exponer cabecera estable en `kernel/include/` si otros subsistemas la consumen (decisión integración).
3. Stub `virtio_blk.c` cuando exista bus VirtIO en QEMU.
# SA-33 — AppImage / SquashFS (reconocimiento)

| Campo | Valor |
|-------|-------|
| Misión | SA-33 reconocimiento AppImage |
| Agente | appimage-squashfs-engineer |
| Estado | COMPLETED (capa de reconocimiento) |
| Fecha | 2026-11-22 |

## Alcance

Solo reconocimiento de formato sobre un **blob en memoria** (callback de lectura). Sin montaje VFS, sin descompresión, sin `exec` ni FUSE.

| Archivo | Rol |
|---------|-----|
| `kernel/fs/squashfs.c` | Validación de superbloque SquashFS 4.0 embebido |
| `kernel/linux/appimage.c` | Detección AppImage tipo 1 / tipo 2 y offset del payload |
| `docs/swarm/sa-33-report.md` | Este informe |

Headers públicos (`squashfs.h`, `appimage.h`) y cableado VFS/loader: **fuera de alcance** (misiones SA-25 / SA-30).

## API (símbolos de enlace)

### `metta_io_blob_t` (compartido)

Lector abstracto: `read(ctx, offset, buf, len)` + `size` total del archivo.

### SquashFS — `kernel/fs/squashfs.c`

| Símbolo | Semántica |
|---------|-----------|
| `squashfs_magic_valid(magic)` | Acepta `0x73717368` / `0x68737173` |
| `squashfs_probe_at(blob, offset, out)` | `METTA_OK` si hay superbloque SquashFS **4.0** coherente |

Rellena `squashfs_image_info_t`: versión, `block_size`, compresión, endian del magic.

### AppImage — `kernel/linux/appimage.c`

| Símbolo | Semántica |
|---------|-----------|
| `appimage_probe(blob, out)` | ELF64 LE en offset 0; luego tipo 2 o tipo 1 |

**Tipo 2:** barrido lineal (hasta 512 KiB, alineado 4) buscando magic `0x41490100` o `0x41490200`; valida `payload_offset` y confirma SquashFS.

**Tipo 1:** lee `uint64_t` en los últimos 8 bytes del archivo; usa ese offset como inicio del SquashFS.

`metta_appimage_info_t` incluye `kind`, offsets, longitud de payload (tipo 1 estimada hasta el footer) y `squashfs` anidado.

## Errores (`metta_status_t`)

| Código | Uso |
|--------|-----|
| `METTA_OK` | AppImage reconocido + SquashFS válido |
| `METTA_ERR_NOT_FOUND` | No AppImage o sin SquashFS en offset candidato |
| `METTA_ERR_CORRUPT` | ELF inválido o superbloque incoherente |
| `METTA_ERR_RANGE` / `METTA_ERR_INVALID_ARG` | Parámetros o lectura fuera de blob |

## Criterios de validación SquashFS

- Magic conocido.
- `version_major/minor` = **4.0** (rango acotado en fuente).
- `block_size` potencia de dos distinta de cero.
- `bytes_used` distinto de cero.

## Limitaciones conocidas

1. **Tipo 2:** búsqueda por scan, no parser completo de secciones ELF `.appimage` (suficiente para reconocimiento temprano).
2. **Compresión / ID tables:** no se interpretan; solo el superbloque.
3. **Arquitectura ELF:** se exige ELF64 LSB; `e_machine == EM_X86_64` se registra pero no bloquea (binarios multi-arch futuros).
4. **Sin prueba en QEMU** — depende de VFS (SA-25) y lectura de archivos desde userspace Linux.
5. **Duplicación de `metta_io_blob_t`** en ambos `.c` hasta existir header compartido.

## Pruebas

| Prueba | Resultado |
|--------|-----------|
| `gcc -ffreestanding -Wpedantic` sobre `squashfs.c` + `appimage.c` | **No ejecutada** — `gcc` ausente en WSL (mismo `INFRA_FAIL` que SA-35) |
| AppImage real en MettaOS | Pendiente (VFS + storage) |

### Verificación local sugerida

```bash
wsl bash -lc 'cd /mnt/c/Users/USER/Desktop/MettaOS && \
  gcc -std=gnu11 -ffreestanding -fno-stack-protector -fno-builtin \
      -Wall -Wextra -Wpedantic -m64 -mcmodel=kernel -mno-red-zone \
      -Ikernel/include -c kernel/fs/squashfs.c -o /tmp/squashfs.o && \
  gcc -std=gnu11 -ffreestanding -fno-stack-protector -fno-builtin \
      -Wall -Wextra -Wpedantic -m64 -mcmodel=kernel -mno-red-zone \
      -Ikernel/include -c kernel/linux/appimage.c -o /tmp/appimage.o'
```

## Dependencias / siguientes pasos

| Misión | Integración |
|--------|-------------|
| SA-25 (storage/VFS) | Proveer `metta_io_blob_t` leyendo disco |
| SA-30 (ELF loader) | `execve` sobre AppImage tipo 2 tras montar squash interno |
| SA-32 / SA-31 | Runtime Linux invoca `appimage_probe` antes de lanzar payload |

## Conclusión

Capa de **reconocimiento** AppImage + SquashFS implementada en los dos archivos del contrato SA-33. Lista para enlazar con VFS y loader cuando existan lectores de archivo en el kernel.
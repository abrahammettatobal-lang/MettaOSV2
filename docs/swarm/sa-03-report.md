# SA-03 — Build system (toolchain)

**Agente:** toolchain-build-engineer  
**Estado:** COMPLETED (compilación WSL verificada 2026-11-22)  
**Fecha:** 2026-11-22

## Entregables

| Archivo | Rol |
|---------|-----|
| `Makefile` | `all`, `kernel-only`, `iso`, `run`, `clean`, `print-objs` |
| `linker.ld` | Enlace freestanding @ 1 MiB, sección `.multiboot_header` |
| `.gitignore` | `build/`, artefactos `*.o` / `*.iso` |
| `boot/grub/grub.cfg` | Stub `multiboot2 /boot/mettaos.bin` |
| `kernel/lib/string.c` | `memcpy` / `strcmp` / … para enlace `-nostdlib` |

## Makefile (resumen)

- **Kernel:** `boot/*.S`, `kernel/**/*.c` (profundidad 3).
- **MDE:** wildcards `mde/**/*.c`, excluye `mde/apps/`; target `MDE_CFLAGS` con `-msse -msse2` (animaciones con `float`).
- **`kernel-only`:** `BUILD_MDE=0` → sin objetos MDE.
- **Preinclude:** `-include stddef.h` en `CFLAGS` (NULL / `size_t` freestanding).
- **Salida:** `build/mettaos.bin`, `build/mettaos.iso`.

## Error inicial (BUILD_FAIL)

Tras SA-02 toolchain OK, el primer fallo en `make kernel-only` fue en `kernel.c`:

```
kernel/include/proc/types.h:21:5: error: redeclaration of enumerator ‘METTA_OK’
kernel/include/metta/types.h:28:5: note: previous definition ...
kernel/include/proc/types.h:31:3: error: conflicting types for ‘metta_status_t’
```

Causa: duplicación de contratos en `proc/types.h` + `proc/vmm.h` frente a `metta/types.h` + `metta/vmm.h`.

## Correcciones de build (mínimas, toolchain-coordinadas)

| Área | Cambio |
|------|--------|
| Headers proc | `proc/types.h` → `#include "metta/types.h"`; `proc/vmm.h` → `#include "metta/vmm.h"` |
| Contrato | `METTA_MEM_*` en `metta/types.h`; `metta_s32`; límites PMM en `metta/pmm.h` |
| Include guards | `kernel/include/types.h` → `METTA_FREESTANDING_TYPES_H`; CPU usa `<stdint.h>` |
| `boot_info.h` | `#include "metta/types.h"` |
| Linux errno | `syscall_errno.h` reexporta `linux/runtime.h` (macros vs enum) |
| VMM | `vmm.c`: `metta/pmm.h` + `pmm_frame_alloc` |
| MFS | tamaños packed, `blkdev` struct, prototipo `mfs_vfs_mkdir` |
| Link | `kernel/lib/string.c` |
| Makefile MDE | `MDE_CFLAGS` con SSE2; `desktop.c` → `#include "mde.h"` |
| MDE wm | `chrome.c` fixes; basura de agente eliminada de `window.c` |

## Prueba real WSL (2026-11-22)

```bash
cd /mnt/c/Users/USER/Desktop/MettaOS
make clean && make kernel-only   # exit 0
make all                         # exit 0
make iso                         # exit 0 (artefacto existente / up-to-date)
```

Salida de enlace (`make all`):

```
[LD]  .../build/mettaos.bin (BUILD_MDE=1)
[OK]  kernel: .../build/mettaos.bin
```

Artefactos:

```
build/mettaos.bin: ELF 64-bit LSB executable, x86-64, statically linked (~244 KiB)
build/mettaos.iso: ~5.6 MiB
```

`scripts/smoke-build.sh` → esperado **PASS** con toolchain SA-02 instalado.

## Objetos enlazados (aprox.)

| Conjunto | Cantidad |
|----------|----------|
| Boot `.S` | 2 |
| Kernel `.c` | 45+ (incl. `lib/string.c`, MFS, linux, MDE kernel stubs) |
| MDE (sin `mde/apps/`) | ~22 |

## QEMU

```bash
make run
# qemu-system-x86_64 -m 4096 -serial stdio -cdrom build/mettaos.iso
```

## Criterio de cierre SA-03

- [x] Makefile + linker + grub stub + `.gitignore`
- [x] Wildcards kernel + MDE (sin apps)
- [x] Target `kernel-only`
- [x] `make kernel-only` exit 0 en WSL
- [x] `make all` exit 0 en WSL
- [x] `make iso` genera / mantiene `build/mettaos.iso`

## Dependencias siguientes

- **Runtime:** validar arranque Multiboot2 en QEMU (SA-04 / QA).
- **MDE apps:** regla de enlace userspace separada cuando exista ABI.
- **PMM `.bss`:** ~128 KiB bitmap + tablas (QEMU 4096 MiB) — aceptado en bring-up.
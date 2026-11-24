# MettaOS — pruebas de integración (SA-35)

Este directorio documenta el **smoke de Etapa arranque**: comprobar que la cadena de compilación produce kernel e ISO antes de pruebas en QEMU (MDE, escritorio, etc.).

## Prerrequisitos

- Windows 11 con **WSL2** y Ubuntu, o Ubuntu nativo.
- Dependencias de toolchain: `scripts/wsl-setup.sh` (véase [README.md](../README.md)).

## Smoke Etapa arranque (build)

Ejecutar **desde Ubuntu WSL** en la raíz del repositorio:

```bash
chmod +x scripts/smoke-build.sh
./scripts/smoke-build.sh
```

Los scripts `*.sh` deben usar finales de línea **LF** (WSL). Si `./scripts/smoke-build.sh` falla con `bash\r`, en WSL: `tr -d '\r' < scripts/smoke-build.sh > /tmp/f && mv /tmp/f scripts/smoke-build.sh`.

Desde **PowerShell** (invoca WSL con la ruta montada en `/mnt/c/...`):

```powershell
wsl.exe bash -lc "cd /mnt/c/Users/USER/Desktop/MettaOS && ./scripts/smoke-build.sh"
```

(Ajuste la ruta si el clon no está en el escritorio.)

### Qué hace el smoke

| Paso | Comando | Criterio |
|------|---------|----------|
| 1 | Comprobar `make`, `gcc`, `nasm`, `ld` | Salida `SMOKE_RESULT=INFRA_FAIL`, código **2** si faltan herramientas |
| 2 | `make all` | Genera `build/mettaos.bin` |
| 3 | `make iso` | Genera `build/mettaos.iso` |

Al final imprime líneas parseables:

- `SMOKE_RESULT=PASS|BUILD_FAIL|INFRA_FAIL`
- `MAKE_ALL_EXIT=<número o skipped>`
- `MAKE_ISO_EXIT=<número o skipped>`

**No considerar PASS** si no se ejecutó el script en un entorno con toolchain instalada.

### Códigos de salida de `smoke-build.sh`

| Código | Significado |
|--------|-------------|
| `0` | `SMOKE_RESULT=PASS` |
| `1` | `SMOKE_RESULT=BUILD_FAIL` (make falló) |
| `2` | `SMOKE_RESULT=INFRA_FAIL` (herramientas ausentes) |

## Smoke Etapa arranque (QEMU) — manual

Tras `PASS` en build, validar arranque (no automatizado en SA-35):

```bash
make run
```

El `Makefile` usa QEMU con **6144 MiB** de RAM, ISO y `-serial stdio`. Criterios mínimos de Etapa arranque:

1. GRUB carga `mettaos.bin` sin error fatal.
2. El kernel entra en `kmain` (sin triple fault inmediato).
3. Si hay `printk` por serie, líneas esperadas visibles en la consola donde corre QEMU.

Interrumpir QEMU con `Ctrl+A` luego `X` en la consola de QEMU, o cerrar la ventana según el frontend.

## Relación con otras etapas

- **SA-03** (Makefile): requisito para `BUILD_FAIL` vs éxito real.
- **SA-04+** (boot/kernel): fallos de enlace o runtime aparecen en `make all` o en QEMU.
- MDE Light / FPS / dirty rects: etapas posteriores; no forman parte del smoke SA-35.

## Informes

Estado verificado de build: [docs/swarm/sa-35-report.md](../docs/swarm/sa-35-report.md).
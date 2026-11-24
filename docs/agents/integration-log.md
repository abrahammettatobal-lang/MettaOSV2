# Integration log (QA)

## Coordinador — integraciones de swarm

| Fecha | SA | Acción | Resultado |
|-------|-----|--------|-----------|
| 2026-11-22 | [SA-01](b73c8ab7-8765-4eff-b9a7-72547f86c1a0) | Contratos rev 1 integrados | INTEGRATED |
| 2026-11-22 | [SA-02](fd28b807-2736-4b6a-b01c-2870af959ee9) | Toolchain WSL | INTEGRATED |
| 2026-11-22 | [SA-03](ca753694-35e3-4f9a-a3f3-a1ecd66cb126) | Makefile / make all+iso | VERIFIED |
| 2026-11-22 | [SA-35](8ec72c6b-538c-42a8-b50a-36c0a92b20b3) | Smoke (informe histórico BUILD_FAIL) | SUPERSEDED |
| 2026-11-22 | LG-01…LG-08 | Liquid Glass Composer 2.5 | COMPLETED |
| 2026-11-22 | UI-01…UI-08 | Oleada UI Carbon | CANCELLED |

---

Registro de ejecuciones de smoke y QEMU. **BUILD_PASS** solo con logs de `make all` + `make iso` exit 0. No marcar VERIFIED sin salida de comando real.

| Fecha | Prueba | Entorno | Resultado | Notas |
|-------|--------|---------|-----------|-------|
| 2026-11-22 16:04 | `scripts/smoke-build.sh` | WSL Ubuntu 26.04, sin toolchain | INFRA_FAIL | `make gcc nasm` ausentes; salida `SMOKE_RESULT=INFRA_FAIL`, exit script `2` |
| 2026-11-22 16:14 | `scripts/smoke-build.sh` | WSL Ubuntu 26.04, SA-02 toolchain OK | **BUILD_FAIL** | `SMOKE_RESULT=BUILD_FAIL`, `MAKE_ALL_EXIT=2`, `MAKE_ISO_EXIT=2`; sin `mettaos.bin`/`mettaos.iso` |
| 2026-11-22 16:14 | `make run` (serial) | — | **No ejecutado** | Bloqueado: smoke no PASS |
| 2026-11-22 | `scripts/smoke-build.sh` | WSL post-fix coordinador (float→milli, tipos) | **PASS** | `SMOKE_RESULT=PASS`, `MAKE_ALL_EXIT=0`, `MAKE_ISO_EXIT=0`; `build/mettaos.bin` + `build/mettaos.iso` |

### Log adjunto — BUILD_FAIL (extracto, histórico)

```text
[smoke-build] Ejecutando: make all
[CC]  .../kernel/kernel.c
kernel/include/proc/types.h:21:5: error: redeclaration of enumerator ‘METTA_OK’
kernel/include/metta/types.h:28:5: note: previous definition of ‘METTA_OK’
kernel/include/proc/types.h:31:3: error: conflicting types for ‘metta_status_t’
kernel/kernel.c:91:33: error: passing argument 1 of ‘metta_sec_set_vmm_probe’ from incompatible pointer type
make: *** [Makefile:102: .../build/obj/kernel/kernel.o] Error 1
[smoke-build] make all → código de salida: 2
SMOKE_RESULT=BUILD_FAIL
MAKE_ALL_EXIT=2
MAKE_ISO_EXIT=2
```

Reporte completo: [docs/swarm/sa-35-report.md](../swarm/sa-35-report.md).
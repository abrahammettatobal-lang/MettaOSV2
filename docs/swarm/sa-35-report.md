# SA-35 — Informe Integration QA (harness smoke arranque)

**Fecha:** 2026-11-22  
**Agente:** integration-qa-engineer  
**Última verificación build:** 2026-11-22 (post SA-02 toolchain + SA-03 Makefile)

## Veredicto actual

| Campo | Valor |
|-------|--------|
| **Clasificación QA** | **BUILD_FAIL** |
| `SMOKE_RESULT` | `BUILD_FAIL` |
| `MAKE_ALL_EXIT` | `2` |
| `MAKE_ISO_EXIT` | `2` |
| `build/mettaos.bin` | **Ausente** (fallo en `kernel.c` antes de enlace) |
| `build/mettaos.iso` | **Ausente** |
| QEMU `make run` | **No ejecutado** (requiere PASS en smoke; sin evidencia serial) |

**BUILD_PASS:** no declarado — no hay logs de `make all` + `make iso` en exit 0.

## Prerrequisitos SA-02 / SA-03 (revisión)

| SA | Estado operativo | Evidencia |
|----|------------------|-----------|
| SA-02 | Toolchain en WSL | `which make gcc nasm` → `/usr/bin/make`, `/usr/bin/gcc`, `/usr/bin/nasm` |
| SA-03 | Makefile presente | Targets `all`, `iso`, `run`; `BUILD_MDE`; `kernel-only` |

Infraestructura **desbloqueada** respecto al run anterior (`INFRA_FAIL`).

## Ejecución real — `scripts/smoke-build.sh`

**Comando:**

```bash
wsl bash -lc 'cd /mnt/c/Users/USER/Desktop/MettaOS && bash scripts/smoke-build.sh 2>&1 | tee /tmp/smoke-out.txt'
```

**Entorno:** WSL Ubuntu 26.04.1 LTS, repo `/mnt/c/Users/USER/Desktop/MettaOS`

**Resumen script (tail):**

```text
make: *** [Makefile:102: .../build/obj/kernel/kernel.o] Error 1
[smoke-build] make all → código de salida: 2
[smoke-build] Ejecutando: make iso
(... mismos errores si make intenta rebuild ...)
[smoke-build] make iso → código de salida: 2
SMOKE_RESULT=BUILD_FAIL
MAKE_ALL_EXIT=2
MAKE_ISO_EXIT=2
[smoke-build] FAIL smoke-build (revisar salida de make arriba).
```

**Primer bloque de error (raíz observable):** duplicación de contratos `metta/types.h` vs `proc/types.h` y `metta/vmm.h` vs `proc/vmm.h` al compilar `kernel/kernel.c`:

```text
kernel/include/proc/types.h:21:5: error: redeclaration of enumerator ‘METTA_OK’
kernel/include/metta/types.h:28:5: note: previous definition of ‘METTA_OK’
...
kernel/include/proc/types.h:31:3: error: conflicting types for ‘metta_status_t’
kernel/kernel.c:91:33: error: passing argument 1 of ‘metta_sec_set_vmm_probe’ from incompatible pointer type
```

**Causa probable:** integración paralela swarm (SA-01 contratos en `metta/*` vs stubs `proc/*` SA-09). Fuera de alcance QA salvo fix mínimo de build — pendiente alineación en misiones kernel.

## Historial

### Run 1 (sin toolchain)

```text
SMOKE_RESULT=INFRA_FAIL
MAKE_ALL_EXIT=skipped
MAKE_ISO_EXIT=skipped
```

Código de salida del script: `2`.

## Harness entregado

| Artefacto | Rol |
|-----------|-----|
| `tests/README.md` | Procedimiento smoke Etapa arranque |
| `scripts/smoke-build.sh` | `make all` + `make iso`, códigos parseables |
| `docs/agents/integration-log.md` | Registro cronológico |
| `docs/agents/status.md` | Tablero QA |

## Próximos pasos

1. Resolver conflictos de headers (`proc/` vs `metta/`) hasta `make all` exit 0.
2. Repetir `./scripts/smoke-build.sh` → objetivo `SMOKE_RESULT=PASS` (equivalente a **BUILD_PASS** en integration-log).
3. Tras PASS: `make run` ~10–15 s, capturar `-serial stdio` en integration-log.
4. Etapas MDE: FPS/dirty rects cuando el escritorio esté en la imagen.

## Conclusión

Harness **SA-35** operativo y **smoke ejecutado con toolchain**. Integración build: **BUILD_FAIL documentado con logs** — no se aprueba arranque en QEMU sin ISO válida recién generada.
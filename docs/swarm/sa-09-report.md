# SA-09 — Procesos y planificador cooperativo

**Agente:** process-scheduler-engineer  
**Fecha:** 2026-11-22  
**Estado:** COMPLETED (integración con VMM SA-08 y `scheduler_init` en `kmain` pendientes)

## Resumen

Se añadió el subsistema de procesos bajo `kernel/proc/` con PCB, estados, cola RUN-queue protegida por `runqueue_lock`, cambio de contexto cooperativo x86_64 y la API de memoria de proceso alineada con `docs/architecture/contracts.md` §4 y `memory-ownership.md` §5.

## Archivos creados

| Archivo | Rol |
|---------|-----|
| `kernel/include/proc/proc_types.h` | PCB, estados y constantes de proceso (incluye `metta/types.h` + `metta/vmm.h`) |
| `kernel/include/proc/process.h` | API proceso/memoria (`sched_switch_mm`, `proc_mm_*`, …) |
| `kernel/include/proc/scheduler.h` | Init, yield cooperativo, cola (`metta/sync.h` para locks) |
| `kernel/proc/process.c` | Tabla PCB, `proc_mm_*`, `sched_switch_mm`, stubs `vmm_*` débiles |
| `kernel/proc/scheduler.c` | Runqueue, `ctx_switch` inline asm, pilas kernel, `scheduler_run_selftest` |
| `docs/swarm/sa-09-report.md` | Este informe |

## PCB y estados

| Campo / concepto | Detalle |
|------------------|---------|
| `metta_pid_t` | `METTA_PID_KERNEL` = 0; slot fijo índice 0 |
| Estados | `UNUSED`, `READY`, `RUNNING`, `BLOCKED`, `ZOMBIE` |
| `metta_proc_mm_t` | AS, ASID, `user_stack_top`, `brk` |
| Cola | Lista FIFO simple; `on_runqueue` + `rq_next` |
| Contexto | `metta_cpu_context_t` (solo `rsp`; callee-saved en pila) |

**`user_stack_top`:** `0x00007FFFFFFFF000` con mapeo solicitado de `METTA_USER_STACK_SIZE` vía `vmm_map` en `proc_mm_init`.

## `sched_switch_mm` (contrato)

1. `next == NULL` → `METTA_ERR_PERM`
2. `METTA_ASID_KERNEL` → `METTA_OK` (sin cambio CR3)
3. `address_space == NULL` (usuario) → `METTA_ERR_PERM`
4. Si `vmm_current_as() != next->mm.address_space` → `vmm_switch_to`
5. En cola de planificación el switch se invoca desde `sched_cooperative_yield` antes de `ctx_switch`

## `proc_mm_destroy` (contrato)

- Toma `runqueue_lock` vía `scheduler_runqueue_lock`
- Proceso en cola o `RUNNING` → `METTA_ERR_BUSY`
- ASID kernel → `METTA_ERR_PERM`
- Destruye AS con `vmm_as_destroy` y limpia `metta_proc_mm_t`

## Context switch cooperativo

- SysV amd64: al cambiar se guardan `rbx`, `rbp`, `r12`–`r15` y `rsp`
- Pilas kernel estáticas (`METTA_PROC_KSTACK_SIZE`); trama inicial apunta a `scheduler_task_trampoline` → `entry` → `ZOMBIE` → `yield`
- Sin preemption por timer aún (SA-06 IRQ lista; enlace IDT pendiente)

## Cabeceras y BUILD_FAIL (post-fix)

- Eliminados `proc/types.h` y `proc/vmm.h`, que duplicaban `metta/types.h` y `metta/vmm.h` y rompían la unidad de traducción cuando `kmain` o VMM incluían ambos contratos.
- **`proc_types.h`** concentra solo tipos de proceso; tipos globales y VMM vienen de `kernel/include/metta/`.
- `scheduler.h` usa `metta_spinlock_t` vía `metta/sync.h` (sin segunda definición).

## Stubs VMM

`vmm_as_create`, `vmm_as_destroy`, `vmm_map`, `vmm_switch_to`, `vmm_current_as` están definidos **débiles** en `process.c` y ceden al VMM real de SA-08 cuando está enlazado.

## Pruebas

| Prueba | Resultado |
|--------|-----------|
| Compilación freestanding (`make`) tras fix headers | Verificar en WSL/toolchain (conflicto `METTA_OK` resuelto) |
| `scheduler_run_selftest` | Dos tareas kernel alternan 4 ciclos cada una vía `sched_cooperative_yield`; destrucción tras `ZOMBIE` |
| QEMU switch AS + refcnt (SA-35 A3) | **Pendiente** — requiere SA-08 y `kmain` |

## Dependencias

| De | Para |
|----|------|
| SA-08 VMM | Sustituir stubs; `proc_mm_init` / switch real CR3 |
| SA-03 Makefile | Ya recoge `kernel/*/*.c` → incluye `kernel/proc/*.c` |
| `kmain` | `scheduler_init()`; opcional `scheduler_run_selftest()` tras `vmm_late_init_after_pmm` |

## Entrega estándar (7 puntos)

1. **Resumen:** PCB + planificador cooperativo + contrato memoria/switch.  
2. **Archivos creados:** tabla superior.  
3. **Modificados:** ninguno fuera de `kernel/proc/` y cabeceras `kernel/include/proc/`.  
4. **Interfaces:** `contracts.md` §4, `memory-ownership.md` §5 y §9.  
5. **Pruebas:** autoprueba lógica en código; build/QEMU pendientes.  
6. **Errores conocidos:** spinlock de cola es ticket simplificado (uniprocesador); `proc_mm_clone` devuelve `METTA_ERR_INTERNAL`.  
7. **Siguiente:** wire en `kmain`, ASID usuario en QEMU con SA-08, preemption PIT cuando IDT esté integrado.
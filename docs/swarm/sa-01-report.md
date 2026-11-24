# SA-01 — Informe Kernel Architecture Contracts

| Campo | Valor |
|-------|-------|
| Misión | SA-01 Kernel Architecture |
| subagent_type | architecture-lead |
| Modelo | Auto (coordinador) |
| Estado | COMPLETED |
| Contrato | `METTA_CONTRACT_REV` = 1 |

---

## 1. Archivos creados

| Ruta | Descripción |
|------|-------------|
| `docs/architecture/contracts.md` | Tipos C, APIs, errores (`metta_status_t`), spinlocks/IRQ, PMM↔VMM, VMM↔scheduler, boot↔kmain |
| `docs/architecture/boot-map.md` | Layout físico/virtual, `metta_boot_info_t`, secuencia `kmain`, validación handoff |
| `docs/architecture/memory-ownership.md` | Refcount, ASID, destrucción AS, reservas boot/FB, checklist auditoría |
| `docs/swarm/sa-01-report.md` | Este informe |

**No modificados:** `boot/`, `Makefile`, `mde/`, código kernel.

---

## 2. Resumen

Se congeló la **revisión 1** del contrato de memoria y arranque para un kernel x86_64 freestanding con modelo Multiboot2:

- **PMM↔VMM:** marcos con clase y `refcnt`; VMM es el único que incrementa/decrementa refcount en map/unmap; clases `PAGE_TABLE` / `USER` / estáticas boot.
- **VMM↔Scheduler:** `metta_proc_mm_t`, `sched_switch_mm`, `proc_mm_destroy` con precondiciones de runqueue; ASID 0 reservado al kernel.
- **Boot↔kmain:** `metta_boot_info_t` con magic, `contract_rev`, memmap y FB; orden estricto `validate → pmm_init → vmm_init → vmm_late_init_after_pmm`.

Sincronización: orden de locks `runqueue → as → vmm → pmm`; PMM/VMM definen uso de `metta_spinlock_t` e `metta_irq_save`.

---

## 3. Interfaces (puntero rápido)

| Interfaz | Cabeceras de contrato (futuro) | Documento |
|----------|----------------------------------|-----------|
| PMM↔VMM | `metta/pmm.h`, `metta/vmm.h` | `contracts.md` §3 |
| VMM↔Scheduler | `metta/vmm.h`, `metta/process.h` (memoria) | `contracts.md` §4 |
| Boot↔kmain | `metta/boot_info.h` | `contracts.md` §5, `boot-map.md` |

---

## 4. Dependencias para misiones siguientes

### SA-07 — PMM (physical-memory-engineer)

**Entradas obligatorias del contrato:**

- `pmm_init`, `pmm_frame_alloc`/`free`, `pmm_frame_ref_inc`/`dec`, `pmm_mark_reserved`, `pmm_query_stats`
- Tipos `metta_frame_class_t`, `metta_phys_frame_t`, `metta_frame_desc_t`
- Inicialización desde `metta_mem_region_t` y tipos `METTA_MEM_*` (`boot-map.md`)
- Reservas antes de alloc: kernel image, framebuffer, boot info, `<1 MiB`, regiones `METTA_MEM_RESERVED`
- Lock global `pmm_lock`; poison/refcount underflow → panic

**Bloquea a:** SA-08 (`vmm_late_init_after_pmm` asume PMM operativo).

### SA-08 — VMM (virtual-memory-engineer)

**Entradas obligatorias del contrato:**

- `vmm_init`, `vmm_late_init_after_pmm`, `vmm_as_create`/`destroy`, `vmm_map`/`unmap`/`protect`, `vmm_switch_to`
- HHDM base `0xFFFF800000000000`, fin de identidad baja transitoria (`boot-map.md`)
- Consumo exclusivo de PMM para marcos; refcount en map/unmap (`memory-ownership.md`)
- Locks `vmm_lock`, `as_lock`; orden con scheduler

**Depende de:** SA-07 completo, SA-03 (símbolos linker kernel phys/virt), SA-04 (handoff válido).

**Bloquea a:** SA-09 (switch CR3 y AS usuario).

### SA-09 — Scheduler / procesos (process-scheduler-engineer)

**Entradas obligatorias del contrato:**

- `metta_proc_mm_t`, `proc_mm_init`, `proc_mm_destroy`, `sched_switch_mm`
- ASID ≥ 1 para usuario; `METTA_ASID_KERNEL` inmutable
- Precondición destroy: proceso fuera de runqueue (`METTA_ERR_BUSY` si no)
- Rango usuario y stack (`memory-ownership.md` §5)

**Depende de:** SA-08 (AS y `vmm_switch_to`).

**No implementar en SA-09:** cambios a refcount PMM ni política de tablas de página (escalar a architecture-lead si hay conflicto).

---

## 5. Pruebas de aceptación (referencia SA-35)

| ID | Criterio |
|----|----------|
| A1 | Tras N ciclos map/unmap, `pmm_query_stats` estable (sin fuga de marcos) |
| A2 | `contract_rev` distinto en handoff → kernel no llama `pmm_init` |
| A3 | Switch entre dos procesos de prueba mantiene refcnt coherente |
| A4 | `proc_mm_destroy` con proceso en cola → `METTA_ERR_BUSY` |

---

## 6. Errores / riesgos conocidos

| Riesgo | Mitigación |
|--------|------------|
| SA-03 linker usa otra `kernel_virt_base` | Actualizar `boot-map.md` + `contract_rev` |
| Huge pages / identidad distinta en SA-04 | Boot debe respetar `metta_boot_info_t` y alineaciones |
| SMP temprano | Orden de locks ya fijado; implementación ticket lock en SA-06+ |

---

## 7. Pendientes (fuera de SA-01)

- `docs/architecture/decisions.md` (ADRs formales) — no solicitado en esta misión.
- Cabeceras reales en `kernel/include/metta/*.h` — corresponde a SA-07/08 al implementar.
- Compositor, VFS, Linux ABI — contratos en oleadas posteriores.

---

*Architecture Lead — SA-01 completado.*
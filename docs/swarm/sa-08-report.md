# SA-08 — Virtual Memory Manager (VMM mínimo)

| Campo | Valor |
|-------|-------|
| Misión | SA-08 VMM |
| subagent_type | virtual-memory-engineer |
| Contrato | `METTA_CONTRACT_REV` = 1 |
| Estado | COMPLETED (núcleo init/switch; map/AS en weak SA-09) |

## Resumen

Implementación mínima en `kernel/mm/vmm.c`: **HHDM** en `0xFFFF800000000000`, `vmm_init`, `vmm_late_init_after_pmm`, `vmm_switch_to` (CR3) y `vmm_current_as`. Sustituye los símbolos débiles de `kernel/proc/process.c` para cambio de espacio de direcciones usados por `sched_switch_mm`.

Marcos de tablas HHDM: solo `pmm_alloc_frame()` / `pmm_free_frame()` vía `mm/pmm.h` (SA-07 legado). Sin pool refcount en esta entrega.

## Archivos

| Ruta | Rol |
|------|-----|
| `kernel/mm/vmm.c` | VMM núcleo + stubs `metta_spin_*` (uniprocesador) |
| `kernel/include/metta/vmm.h` | API contrato (ya presente) |
| `kernel/include/metta/types.h` | `METTA_KERNEL_VIRT_BASE`, tipos |
| `docs/swarm/sa-08-report.md` | Este informe |

**Ámbito:** solo `kernel/mm/vmm*`. `vmm_as_create`, `vmm_map`, etc. siguen como **weak** en `process.c`.

## API implementada

```c
metta_status_t vmm_init(void);
metta_status_t vmm_late_init_after_pmm(void);
metta_status_t vmm_switch_to(metta_address_space_t *as);
metta_address_space_t *vmm_current_as(void);
```

| Función | Comportamiento |
|---------|----------------|
| `vmm_init` | CR3 actual = AS kernel; PML4[`256`] → HHDM 4 GiB (huge 2 MiB); `vmm_current` = kernel |
| `vmm_late_init_after_pmm` | Marca acceso vía HHDM; desmapea identidad PML4[0] solo si RIP ≥ base canonical high |
| `vmm_switch_to` | `mov cr3, as->pml4_phys`; actualiza `vmm_current_as` |
| `vmm_current_as` | Puntero al AS activo |

## HHDM

- Base: `METTA_KERNEL_VIRT_BASE` = `0xFFFF800000000000` (`boot-map.md`).
- Índice PML4: 256 (derivado de la base virtual).
- Cobertura: `PMM_MAX_PHYS_BYTES` (4 GiB QEMU).

## Enlace SA-09

`sched_switch_mm` compara `vmm_current_as()` con `next->mm.address_space` y llama `vmm_switch_to` si difieren. Con `kernel/mm/vmm.c` en el enlace, **ya no** se usan los weak que devolvían `METTA_ERR_UNINITIALIZED` para switch/current.

## Pruebas

| Prueba | Resultado |
|--------|-----------|
| `make` / enlace con `proc/*.c` | Ejecutar en entorno WSL/toolchain SA-03 |
| QEMU switch AS usuario | Pendiente (`vmm_as_create` aún weak) |

## Pendientes

- `vmm_map` / `unmap` / `protect`, `vmm_as_create` / `destroy`, `vmm_virt_to_phys`
- Refcount `metta/pmm.h` en map/unmap (`memory-ownership.md`)
- Desmapeo identidad con kernel enlazado en low half (trampolín high-half SA-03)
- Mover `metta_spin_*` a módulo sync dedicado

## Entrega (7 puntos)

1. **Resumen:** VMM mínimo + CR3 para scheduler.
2. **Creados/modificados:** `kernel/mm/vmm.c`, informe.
3. **Interfaces:** `contracts.md` §3.3, `boot-map.md` §3.
4. **Depende de:** SA-07 PMM, tablas boot (`pml4`/`pdpt`/`pd`).
5. **Bloquea:** SA-09 switch CR3 real; SA-09 map sigue en weak.
6. **Errores:** ninguno conocido en código fuente.
7. **Pruebas build:** pendiente verificación local.
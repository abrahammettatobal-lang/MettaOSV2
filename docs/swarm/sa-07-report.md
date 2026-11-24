# SA-07 — Physical Memory Manager (PMM)

**Agente:** physical-memory-engineer  
**Contrato:** `METTA_CONTRACT_REV` = 1 (`docs/architecture/contracts.md` §3)  
**Fecha:** 2026-11-22  
**Estado:** COMPLETED (integración `kmain` → `pmm_init(memmap)` pendiente SA-04)

## Resumen

PMM alineado con SA-01: inicialización desde `metta_mem_region_t[]`, bitmap 4 KiB para **4096 MiB**, refcount por marco, clases `metta_frame_class_t` y reservas según `boot-map.md` / `memory-ownership.md`. Sin VMM.

## Archivos

| Archivo | Rol |
|---------|-----|
| `kernel/include/metta/pmm.h` | API pública §3.2 |
| `kernel/include/metta/types.h` | Tipos compartidos y `METTA_MEM_*` (requerido por contrato PMM) |
| `kernel/mm/pmm.c` | Implementación |
| `docs/swarm/sa-07-report.md` | Este informe |

**Eliminado:** `kernel/include/mm/pmm.h` (API obsoleta pre-SA-01).

## API implementada

| Función | Notas |
|---------|--------|
| `pmm_init(regions, count)` | Todo fuera de mapa → ocupado; solo `METTA_MEM_AVAILABLE` ≥ 1 MiB en free list |
| `pmm_frame_alloc` / `pmm_frame_free` | `refcnt` inicial 1 en alloc; free exige `refcnt==1` |
| `pmm_frame_ref_inc` / `pmm_frame_ref_dec` | Underflow `refcnt` → panic (`METTA_ERR_CORRUPT`) |
| `pmm_mark_reserved` | Marco `METTA_FRAME_RESERVED`, fuera del allocator |
| `pmm_query_stats` | `total_frames` = 4GiB/4K; `free_frames` desde bitmap |
| `pmm_frame_get` / `pmm_frame_phys` | Descriptores para VMM (SA-08) |

**Handles:** `metta_phys_frame_t` = entrada en tabla estática indexada por PFN (`pmm_handles[]`).

## Reglas boot-map / ownership

- `[0, 1 MiB)` siempre reservado al init.
- Tipos `METTA_MEM_KERNEL_IMAGE` → `METTA_FRAME_KERNEL_STATIC` (`refcnt = UINT32_MAX`, no `free`).
- `METTA_MEM_FRAMEBUFFER` y no-AVAILABLE → reserva / clase estática según tipo.
- Metadatos PMM (bitmap, `pmm_slots`, `pmm_handles`) autocautelados como `KERNEL_STATIC`.
- Clases liberables tras `ref_dec` a 0: `USER`, `PAGE_TABLE`, `KERNEL_HEAP` (`memory-ownership.md` §2–3).

## Estructuras internas

- Bitmap: ~128 KiB.
- `pmm_slots[]`: refcnt + clase + flag allocated (uno por marco posible; ~varios MiB en `.bss`, aceptable en QEMU 4G).

## Pruebas

| Prueba | Estado |
|--------|--------|
| Compile freestanding | Pendiente toolchain WSL / `make` |
| alloc/free + refcnt | Pendiente harness SA-35 |
| Coherencia memmap QEMU | Pendiente boot con `metta_boot_info_t` |

## Dependencias

| Agente | Entrega necesaria |
|--------|-------------------|
| SA-04 | `memmap` + `memmap_count` en `metta_boot_info_t` |
| SA-08 | Consumo de `pmm_frame_*` y refcount en map/unmap |
| SA-03 | `-Ikernel/include` y objeto `pmm.c` en enlace |

## Entrega estándar (7 puntos)

1. PMM contrato rev 1 listo para VMM.  
2. Creados: tabla archivos arriba.  
3. Modificados: `pmm.c`, reporte; eliminado `include/mm/pmm.h`.  
4. `contracts.md` §3, `boot-map.md`, `memory-ownership.md`.  
5. Pruebas: no ejecutadas en host actual.  
6. Sin parseo Multiboot en PMM (handoff en boot).  
7. `pmm_lock` ligero con `pause`; sustituir por `metta_spinlock_t` (SA-06) en SMP.
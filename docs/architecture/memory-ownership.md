# MettaOS — Propiedad de memoria y reglas de ciclo de vida

Complemento obligatorio de `contracts.md` para **SA-07 (PMM)**, **SA-08 (VMM)** y **SA-09 (scheduler/procesos)**.

---

## 1. Principios

1. **Un marco físico tiene un único dueño lógico** en cada instante: PMM free list, VMM (vía refcnt), o reserva estática boot.
2. **Las tablas de página son marcos `METTA_FRAME_PAGE_TABLE`**; su contenido es propiedad del VMM del espacio al que pertenecen.
3. **El espacio de direcciones kernel (`METTA_ASID_KERNEL`)** no se destruye; CR3 kernel es global.
4. Violación de refcount o doble liberación → **`panic("memory contract")`** en builds de integración (SA-35).

---

## 2. Matriz de propiedad (marco físico)

| Estado | Quién puede mutar | Transiciones permitidas |
|--------|-------------------|-------------------------|
| Reservado boot | Boot / `pmm_mark_reserved` | → nunca a free list sin revisión de contrato |
| Libre PMM | `pmm_frame_alloc` | Libre → asignado (clase X) |
| Asignado sin mapa | PMM holder | → mapeado (VMM `ref_inc`) o `pmm_frame_free` si refcnt==1 y clase lo permite |
| Mapeado en AS | VMM + refcnt | `unmap` → ref_dec → posible free |
| Huérfano (refcnt 0, aún en PTE) | — | **PROHIBIDO** — bug VMM |

---

## 3. Refcount (PMM ↔ VMM)

```text
refcnt inicial en pmm_frame_alloc: 1  (dueño PMM/VMM holder)

vmm_map con marco nuevo:
  pmm_frame_alloc → refcnt = 1
  instalar PTE
  (opcional) pmm_frame_ref_inc si mapeo compartido mismo frame — etapa 1: no compartir user frames

vmm_unmap:
  limpiar PTE
  pmm_frame_ref_dec
  if refcnt == 0 → pmm_frame_free (solo METTA_FRAME_USER y PAGE_TABLE vacías)
```

**Tablas de página:**

- Cada nivel (PML4/PDPT/PD/PT) es un marco `METTA_FRAME_PAGE_TABLE`.
- Al liberar un subárbol vacío, VMM devuelve marcos de tabla a PMM con `ref_dec` en cadena (orden hijo → padre).

**Kernel static:** `refcnt` puede quedar en valor fijo alto o flag `no_free`; `pmm_frame_free` devuelve `METTA_ERR_PERM`.

---

## 4. Espacios de direcciones (VMM ↔ Scheduler)

| Objeto | Creador | Destructor | Condición destroy |
|--------|---------|------------|-------------------|
| `METTA_ASID_KERNEL` | `vmm_init` | nunca | — |
| AS usuario | `vmm_as_create` en `proc_mm_init` | `vmm_as_destroy` en `proc_mm_destroy` | proceso sin CPU, no en runqueue |
| `metta_proc_mm_t` | SA-09 | SA-09 | tras `vmm_as_destroy` OK |

**Scheduler:**

- `sched_switch_mm` **presta** CPU; no transfiere propiedad del AS.
- El AS sigue ligado al `metta_process_t` hasta `proc_mm_destroy`.
- No se permite que dos procesos compartan un AS en revisión 1 (sin CLONE_VM).

---

## 5. Rangos virtuales y ownership lógico

### Kernel

| Rango | Contenido | Mutador |
|-------|-----------|---------|
| HHDM kernel | imagen, bss, heap | allocator kernel (futuro kmalloc sobre PMM) |
| Ventana VMM alta | mapeos efímeros | solo `vmm_*` con `vmm_lock` |

### Usuario (por AS)

| Rango | Política |
|-------|----------|
| `0x0000000000400000` … `user_stack_top - gap` | mmap/loader futuro; SA-09 fija `user_stack_top` |
| Stack usuario | mapeado por `proc_mm_init`; RW- + METTA_PERM_USER |
| NULL page | no mapeada (trap) |

**Gap mínimo bajo stack:** 1 página no mapeada para underflow de stack.

---

## 6. Boot artifacts

| Artefacto | Propietario tras `pmm_init` |
|-----------|------------------------------|
| `metta_boot_info_t` original en RAM baja | Copia en kernel recomendada; físico original → reservado o reclaim solo si boot lo marca AVAILABLE |
| Memmap Multiboot | Igual: copiar a `.bss` kernel antes de reclaim |
| Identidad baja temporal | VMM desmapea; marcos devueltos a PMM si eran solo PTE bootstrap |

Boot **no** llama `pmm_frame_free`; solo entrega descriptores.

---

## 7. Sincronización y carreras

| Escenario | Regla |
|-----------|-------|
| `pmm_frame_alloc` + IRQ | IRQs off o `pmm_lock`; ISR no alloc en rev 1 |
| `vmm_unmap` + switch | `sched_switch_mm` no corre en paralelo con destroy del mismo AS; `proc_mm_destroy` toma `runqueue_lock` |
| Doble `ref_dec` | detectar en PMM → `METTA_ERR_CORRUPT` + panic |
| Uso tras free | PMM poison pattern en debug (SA-35); no contrato release |

---

## 8. Framebuffer y MMIO

- Marcos que cubren FB: **reservados**, clase `METTA_FRAME_RESERVED`, no refcnt usuario.
- VMM puede mapear FB en kernel HHDM o ventana dedicada **sin** pasar por free list de usuario.
- Propiedad: subsistema gráfico (SA-14) lee `metta_boot_info_t`; no unmap sin desinit display.

---

## 9. Auditoría (checklist SA-07/08/09)

**SA-07 PMM**

- [ ] Bitmap/lista coherente con memmap tipos.
- [ ] `ref_dec` no baja de 0.
- [ ] Reservas kernel/FB/boot_info antes del primer `alloc` usuario.

**SA-08 VMM**

- [ ] Cada PTE usuario tiene marco con refcnt ≥ 1.
- [ ] Destroy AS libera todo el subárbol usuario + tablas.
- [ ] CR3 kernel estable tras `vmm_late_init_after_pmm`.

**SA-09 Scheduler**

- [ ] `proc_mm_destroy` solo con proceso detached.
- [ ] Switch no pierde AS actual en TLB sin `invlpg`/flush según política VMM.
- [ ] `METTA_PID_KERNEL` usa siempre ASID 0.

---

## 10. Política ante errores

| Error | Acción |
|-------|--------|
| `METTA_ERR_NO_MEMORY` en `proc_mm_init` | fallo crear proceso; sin rollback parcial sin destroy |
| `METTA_ERR_BUSY` en destroy | scheduler reintenta tras deschedule |
| `METTA_ERR_CORRUPT` en PMM/VMM | panic inmediato |

---

*Revisión 1 — SA-01 Architecture Lead.*
# MettaOS — Contratos de subsistema del kernel (etapa SA-01)

**Versión del contrato:** `METTA_CONTRACT_REV` = `1`  
**Ámbito:** kernel x86_64 freestanding, sin runtime de usuario en esta etapa.  
**Congelamiento:** cambios incompatibles requieren incremento de revisión y actualización de SA-07/08/09.

Convenciones globales:

- Cabeceras públicas bajo `kernel/include/metta/` (solo contrato; implementación en `kernel/`).
- Tipos de tamaño fijo: `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`.
- Alineación de marcos y tablas de páginas: 4096 bytes (`METTA_PAGE_SIZE`).
- Errores: `metta_status_t` (nunca códigos errno de Linux en el kernel nativo).
- Concurrencia temprana: una CPU lógica hasta que IRQ/timer habiliten preemption (SA-06); contratos ya definen reglas para extensión SMP.

---

## 1. Tipos y errores compartidos

```c
/* kernel/include/metta/types.h */
#define METTA_PAGE_SIZE          4096u
#define METTA_PAGE_SHIFT         12

typedef uint8_t   metta_u8;
typedef uint16_t  metta_u16;
typedef uint32_t  metta_u32;
typedef uint64_t  metta_u64;
typedef intptr_t  metta_ssize_t;
typedef uintptr_t metta_phys_addr_t;
typedef uintptr_t metta_virt_addr_t;

typedef struct {
    metta_phys_addr_t base;
    metta_u64         length;
    metta_u32         type;   /* ver boot-map.md: METTA_MEM_* */
    metta_u32         reserved;
} metta_mem_region_t;

typedef enum {
    METTA_OK = 0,
    METTA_ERR_INVALID_ARG,
    METTA_ERR_NO_MEMORY,
    METTA_ERR_NOT_FOUND,
    METTA_ERR_BUSY,
    METTA_ERR_PERM,
    METTA_ERR_UNINITIALIZED,
    METTA_ERR_CORRUPT,
    METTA_ERR_RANGE,
    METTA_ERR_INTERNAL
} metta_status_t;

/* Marca de recurso no asignado */
#define METTA_PHYS_NONE  ((metta_phys_addr_t)0)
#define METTA_VIRT_NONE  ((metta_virt_addr_t)0)
```

**Reglas:**

- Las funciones que pueden fallar devuelven `metta_status_t`; salidas por puntero solo se escriben en `METTA_OK`.
- `METTA_ERR_CORRUPT` implica **panic** en PMM/VMM salvo documentación explícita de recuperación.
- Punteros kernel a memoria de dispositivo usan `volatile` en el driver; PMM/VMM no asumen MMIO salvo marcos reservados en el mapa de arranque.

---

## 2. Sincronización (contrato mínimo)

```c
/* kernel/include/metta/sync.h */
typedef struct {
    volatile metta_u32 owner;   /* ticket o ticket+1; ver implementación SA-06+ */
    volatile metta_u32 depth;   /* recursión permitida solo en mismo CPU */
} metta_spinlock_t;

#define METTA_SPINLOCK_INIT { 0, 0 }

void metta_spin_lock(metta_spinlock_t *lock);
void metta_spin_unlock(metta_spinlock_t *lock);
bool metta_spin_trylock(metta_spinlock_t *lock);

/* Deshabilita IRQ en CPU local; anidamiento contado */
metta_u32 metta_irq_save(void);
void    metta_irq_restore(metta_u32 flags);
```

| Subsistema | Bloqueos | IRQ |
|------------|----------|-----|
| PMM | `pmm_lock` en todas las rutas que mutan bitmap/lista | `alloc`/`free` en IRQ: solo si `metta_irq_save()` activo o bajo `pmm_lock` + sin bloqueo con VMM |
| VMM | `vmm_lock` por árbol de tablas del kernel; `as_lock` por espacio de direcciones | Cambio de CR3: no en ISR excepto switch de proceso vía scheduler |
| Scheduler | `runqueue_lock`; switch de AS solo bajo contrato VMM↔sched | `schedule()` nunca con `pmm_lock` tomado |

**Orden de bloqueo (deadlock):** `runqueue_lock` → `as_lock` → `vmm_lock` → `pmm_lock`. Nunca invertir.

---

## 3. PMM ↔ VMM

### 3.1 Identificadores y marcos

```c
/* kernel/include/metta/pmm.h */
typedef struct metta_phys_frame metta_phys_frame_t;

typedef metta_u64 metta_frame_ref_t;  /* 0 = handle inválido */

typedef enum {
    METTA_FRAME_KERNEL_STATIC = 0,  /* reservado en boot; nunca en free list */
    METTA_FRAME_KERNEL_HEAP,
    METTA_FRAME_PAGE_TABLE,
    METTA_FRAME_USER,
    METTA_FRAME_DMA,                /* reservado; solo SA futuro */
    METTA_FRAME_RESERVED
} metta_frame_class_t;

typedef struct {
    metta_phys_addr_t   addr;       /* alineado a METTA_PAGE_SIZE */
    metta_frame_class_t class;
    metta_u32           refcnt;     /* VMM incrementa/decrementa vía API */
} metta_frame_desc_t;
```

### 3.2 API PMM (consumida por VMM y boot)

```c
metta_status_t pmm_init(const metta_mem_region_t *regions, metta_u32 count);

metta_status_t pmm_frame_alloc(metta_frame_class_t class, metta_phys_frame_t **out);
metta_status_t pmm_frame_free(metta_phys_frame_t *frame);

metta_status_t pmm_frame_get(metta_phys_frame_t *frame, metta_frame_desc_t *out);
metta_phys_addr_t pmm_frame_phys(metta_phys_frame_t *frame);

/* Refcount: solo VMM y subsystems autorizados */
metta_status_t pmm_frame_ref_inc(metta_phys_frame_t *frame);
metta_status_t pmm_frame_ref_dec(metta_phys_frame_t *frame); /* free si refcnt==0 y class permite */

metta_status_t pmm_mark_reserved(metta_phys_addr_t base, metta_u64 length);
metta_status_t pmm_query_stats(metta_u64 *free_frames, metta_u64 *total_frames);
```

**Errores esperados:**

| Función | `NO_MEMORY` | `INVALID_ARG` | `CORRUPT` |
|---------|-------------|---------------|-----------|
| `pmm_frame_alloc` | sin marcos libres de clase | `out==NULL` | bitmap inconsistente |
| `pmm_frame_ref_dec` | — | frame ya liberado | refcnt underflow |

**Contrato de clase:** VMM solo pide `METTA_FRAME_PAGE_TABLE` y `METTA_FRAME_USER` (mapeo). El boot reserva `METTA_FRAME_KERNEL_STATIC` antes de `pmm_init`.

### 3.3 API VMM (consumida por scheduler y syscall layer futuro)

```c
/* kernel/include/metta/vmm.h */
typedef struct metta_address_space metta_address_space_t;
typedef metta_u32 metta_asid_t;

#define METTA_ASID_KERNEL  0u
#define METTA_ASID_INVALID UINT32_MAX

typedef enum {
    METTA_PERM_NONE  = 0,
    METTA_PERM_READ  = 1u << 0,
    METTA_PERM_WRITE = 1u << 1,
    METTA_PERM_EXEC  = 1u << 2,
    METTA_PERM_USER  = 1u << 3
} metta_page_perm_t;

typedef struct {
    metta_virt_addr_t virt;
    metta_phys_frame_t *phys;   /* NULL = demanda cero anónima (futuro) */
    metta_page_perm_t perms;
    metta_u64         flags;    /* METTA_MAP_* */
} metta_map_request_t;

#define METTA_MAP_FIXED   (1u << 0)
#define METTA_MAP_NOEXEC  (1u << 1)

metta_status_t vmm_init(void);
metta_status_t vmm_late_init_after_pmm(void);

/* Espacios */
metta_status_t vmm_as_create(metta_address_space_t **out, metta_asid_t *asid_out);
metta_status_t vmm_as_destroy(metta_address_space_t *as);

metta_status_t vmm_map(metta_address_space_t *as, const metta_map_request_t *req);
metta_status_t vmm_unmap(metta_address_space_t *as, metta_virt_addr_t virt, metta_u64 length);
metta_status_t vmm_protect(metta_address_space_t *as, metta_virt_addr_t virt,
                           metta_u64 length, metta_page_perm_t perms);

metta_status_t vmm_switch_to(metta_address_space_t *as);  /* carga CR3; solo scheduler o trap return */
metta_address_space_t *vmm_current_as(void);
metta_phys_addr_t vmm_virt_to_phys(metta_address_space_t *as, metta_virt_addr_t virt);
```

**Flujo PMM→VMM en `vmm_map`:**

1. Si `req.phys == NULL`, VMM llama `pmm_frame_alloc(METTA_FRAME_USER, &frame)` (o PAGE_TABLE para nodos de tabla).
2. VMM instala PTE; llama `pmm_frame_ref_inc` por cada marco físico mapeado en el AS.
3. En `vmm_unmap`, VMM limpia PTE y `pmm_frame_ref_dec` por marco usuario; tablas vacías devuelven marcos `METTA_FRAME_PAGE_TABLE` a PMM.

**Errores cruzados:**

- `vmm_map` → `METTA_ERR_NO_MEMORY`: falló `pmm_frame_alloc` o no hay hueco virtual.
- `vmm_unmap` → `METTA_ERR_RANGE`: rango no alineado a página o no mapeado.

---

## 4. VMM ↔ Scheduler (procesos)

```c
/* kernel/include/metta/process.h — solo memoria; resto en SA-09 */
typedef metta_u64 metta_pid_t;
#define METTA_PID_KERNEL 0ull

typedef struct metta_process metta_process_t;

typedef struct {
    metta_address_space_t *address_space;
    metta_asid_t           asid;
    metta_virt_addr_t      user_stack_top;   /* exclusión del rango mapeable */
    metta_virt_addr_t      brk;              /* break heap usuario (futuro) */
} metta_proc_mm_t;

metta_status_t proc_mm_init(metta_process_t *proc);
metta_status_t proc_mm_clone(const metta_process_t *parent, metta_process_t *child); /* futuro fork */
metta_status_t proc_mm_destroy(metta_process_t *proc);

/* Solo scheduler + trap return */
metta_status_t sched_switch_mm(metta_process_t *next);
```

**Contrato de cambio de contexto:**

1. `sched_switch_mm` valida que `next->mm.address_space` esté activo o llama `vmm_switch_to`.
2. No se destruye un AS con hilos en cola; `proc_mm_destroy` exige proceso `ZOMBIE` o sin CPU asignada (`METTA_ERR_BUSY`).
3. Kernel mapea identidad + ventana superior fija (`boot-map.md`); cada proceso usuario obtiene AS nuevo con `vmm_as_create` (ASID ≥ 1).

**Errores:**

| API | `BUSY` | `PERM` |
|-----|--------|--------|
| `proc_mm_destroy` | proceso en runqueue | AS del kernel |
| `sched_switch_mm` | — | `next==NULL` o AS corrupto |

---

## 5. Boot ↔ kmain

```c
/* kernel/include/metta/boot_info.h */
#define METTA_BOOT_MAGIC 0x4D455454415F424Fu /* "METTA_BO" */

typedef struct {
    metta_u64           magic;
    metta_u32           contract_rev;  /* debe coincidir con METTA_CONTRACT_REV */
    metta_u32           flags;
    metta_phys_addr_t   framebuffer_phys;
    metta_u32           fb_width;
    metta_u32           fb_height;
    metta_u32           fb_pitch;
    metta_u32           fb_bpp;
    metta_mem_region_t *memmap;
    metta_u32           memmap_count;
    metta_phys_addr_t   kernel_phys_start;
    metta_phys_addr_t   kernel_phys_end;
    metta_virt_addr_t     kernel_virt_base;  /* HHDM o fija según boot-map */
} metta_boot_info_t;

/* Arranque */
void kmain(metta_boot_info_t *info) __attribute__((noreturn));

metta_status_t boot_handoff_validate(const metta_boot_info_t *info);
```

**Contrato:**

- Boot deja `info` en memoria **identidad mapeada** válida hasta que `vmm_late_init_after_pmm` establezca mapeo permanente del kernel.
- `kmain` debe llamar en orden: `boot_handoff_validate` → `pmm_init` → `vmm_init` / `vmm_late_init_after_pmm` → subsistemas (SA-06+).
- Si `magic` o `contract_rev` fallan → `panic` sin intentar `pmm_init`.

Ver detalle físico/virtual en `boot-map.md` y reglas de propiedad en `memory-ownership.md`.

---

## 6. Tabla resumen de interfaces

| Par | Productor | Consumidor | Artefacto de verdad |
|-----|-----------|------------|---------------------|
| PMM↔VMM | PMM frames + refcnt | VMM map/unmap | `pmm_frame_desc_t.refcnt`, PTE |
| VMM↔Scheduler | VMM AS/CR3 | `sched_switch_mm` | `metta_proc_mm_t` |
| Boot↔kmain | Bootloader | `kmain` | `metta_boot_info_t` (solo lectura post-validate, copia interna permitida) |

---

## 7. Pruebas de aceptación (SA-35 / integración)

- SA-07: `pmm_frame_alloc`/`free` + refcnt sin fugas en ciclo map/unmap simulado.
- SA-08: `vmm_map`/`unmap` respeta orden de bloqueo con PMM bajo stress de alloc.
- SA-09: switch entre dos AS de prueba sin fugas de refcnt; `METTA_ASID_KERNEL` inmutable.

---

*Fin contrato revisión 1 — Architecture Lead, SA-01.*
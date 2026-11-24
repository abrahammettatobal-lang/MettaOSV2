# SA-11 — Kernel Security (validación punteros + panic)

| Campo | Valor |
|-------|-------|
| Misión | SA-11 Kernel Security |
| subagent_type | kernel-security-engineer |
| Estado | COMPLETED |
| Contrato | `METTA_CONTRACT_REV` = 1 |
| Ámbito código | `kernel/security/` únicamente |

---

## 1. Archivos creados

| Ruta | Descripción |
|------|-------------|
| `kernel/security/validate.c` | Rangos usuario, canónico x86_64, guardia bajo stack, C-string acotado, sonda VMM opcional |
| `kernel/security/panic.c` | Política halt / log+halt, puente `METTA_ERR_CORRUPT` → panic |
| `kernel/security/include/metta/validate.h` | API pública de validación |
| `kernel/security/include/metta/panic.h` | API panic y diagnóstico registrable |
| `kernel/security/include/metta/security.h` | Umbrella |
| `docs/swarm/sa-11-report.md` | Este informe |

**No modificados:** `mm/`, `proc/`, `syscall/`, `docs/architecture/` (auditoría solo lectura).

---

## 2. Auditoría — huecos en contratos de memoria

Revisión contra `docs/architecture/contracts.md` y `memory-ownership.md §5–§10`.

| Hueco | Riesgo | Mitigación SA-11 |
|-------|--------|------------------|
| `ptr + len` sin comprobación de overflow | Bypass del tope usuario | `metta_sec_range_core`: `len > METTA_USER_VIRT_MAX - start + 1` |
| Punteros kernel/HHDM en syscall | Lectura/escritura espacio kernel | Rechazo con `METTA_ERR_PERM` si `METTA_VIRT_UPPER_REJECT_MASK` |
| Página NULL (0–4 MiB) | Deref silencioso | `METTA_USER_VIRT_MIN` = `0x400000` |
| Gap bajo stack usuario (1 página) | Underflow stack | `metta_sec_check_stack_policy` + `metta_sec_set_context` |
| `METTA_ERR_CORRUPT` sin acción fija | Contrato ambiguo post-PMM/VMM | `metta_panic_on_corrupt` → `metta_panic` + `hlt` |
| Cadenas usuario sin límite / deref kernel | Fault o escaneo infinito | `metta_sec_validate_user_cstr` reserva rango; NUL post-`copy_from_user` |
| Validación sin AS actual | Falsa confianza en rangos | `metta_sec_set_vmm_probe` (SA-08/09 enlazan sonda PTE) |

---

## 3. API (consumidores: syscall SA-10, scheduler SA-09)

### Inicialización

```c
metta_sec_init();
metta_sec_set_context(&(metta_sec_context_t){ .user_stack_top = proc->mm.user_stack_top });
metta_sec_set_vmm_probe(vmm_user_range_probe); /* cuando exista VMM */
metta_panic_register_diag(printk_panic_diag);  /* opcional SA-12 */
```

### Validación (errores → **sin panic**; syscall devuelve `-EFAULT` / `METTA_ERR_*`)

```c
metta_status_t metta_sec_validate_user_ptr(const void *ptr);
metta_status_t metta_sec_validate_user_range(const void *ptr, metta_u64 len);
metta_status_t metta_sec_validate_user_range_access(const void *ptr, metta_u64 len,
                                                      metta_user_access_t access);
metta_status_t metta_sec_validate_user_cstr(const char *ptr, metta_u64 max_scan);
metta_status_t metta_sec_fill_user_range(const void *ptr, metta_u64 len,
                                           metta_user_range_t *out);
```

### Panic (solo contrato kernel)

```c
metta_status_t metta_panic_on_corrupt(metta_status_t st, const char *where);
__attribute__((noreturn)) void metta_panic(metta_panic_class_t cls,
                                             const char *where, const char *detail);
```

| Clase | Uso |
|-------|-----|
| `METTA_PANIC_MEMORY_CONTRACT` | refcount, doble free, PTE huérfana |
| `METTA_PANIC_BOOT` | handoff inválido (junto `boot_handoff_validate`) |
| `METTA_PANIC_SECURITY_INTERNAL` | invariante rota en propio subsistema |

---

## 4. Contrato syscall ↔ seguridad (futuro SA-10)

1. **Siempre** validar rango antes de `copy_from_user` / `copy_to_user`.
2. `METTA_ERR_PERM` / `METTA_ERR_RANGE` / `METTA_ERR_INVALID_ARG` → error al proceso; **nunca** panic.
3. PMM/VMM devuelven `METTA_ERR_CORRUPT` → `metta_panic_on_corrupt` en el caller kernel.
4. `metta_sec_init()` obligatorio antes del primer syscall usuario.

---

## 5. Compilación unitaria (SA-03)

```bash
INC="-Ikernel/include -Ikernel/security/include"
FLAGS="-ffreestanding -fno-stack-protector -mno-red-zone -mcmodel=kernel -Wall -Wextra -c"

x86_64-elf-gcc $FLAGS $INC -o panic.o    kernel/security/panic.c
x86_64-elf-gcc $FLAGS $INC -o validate.o kernel/security/validate.c
```

---

## 6. Pruebas sugeridas (SA-35)

| ID | Caso | Esperado |
|----|------|----------|
| S1 | `(void*)0x1000`, len 1 | `METTA_ERR_RANGE` |
| S2 | `(void*)0xFFFF800000001000`, len 8 | `METTA_ERR_PERM` |
| S3 | `start=0x500000`, `len=0xFFFFFFFFFFFFFFFF` | `METTA_ERR_RANGE` (overflow) |
| S4 | Stack top `0x7F000000`, rango incluye `top - PAGE` | `METTA_ERR_RANGE` |
| S5 | `metta_panic_on_corrupt(METTA_ERR_CORRUPT, "t")` | halt (QEMU detenido) |
| S6 | Sonda VMM mock: rango no mapeado | `METTA_ERR_PERM` vía probe |

---

## 7. Dependencias

| De | Para |
|----|------|
| SA-01 contratos | tipos `metta_*`, rangos usuario |
| SA-08 VMM | implementar `metta_sec_vmm_probe_fn` |
| SA-09 Scheduler | publicar `user_stack_top` en `metta_sec_set_context` |
| SA-12 Debug | `metta_panic_register_diag` → serial/printk |

---

*Kernel Security Engineer — SA-11 completado.*
# SA-10 — Capa de syscalls nativos MettaOS

**Agente:** native-syscall-engineer  
**Fecha:** 2026-11-22  
**Estado:** COMPLETED (stub punteros; sin trap SYSCALL en CPU hasta SA-05/09)

## Resumen

Se añadió el **dispatcher de syscalls nativos** (no Linux ABI): tabla de 256 entradas, números `METTA_SYS_*`, convención de registros x86_64 documentada y **validación de punteros usuario en modo stub** (rango canónico bajo, sin consulta PTE). Incluye banda reservada **MDE** (`0x100+`) para handshake y ventanas gráficas futuras.

## Archivos creados

| Archivo | Rol |
|---------|-----|
| `kernel/include/syscall/metta_abi.h` | ABI pública, números, errores `METTA_ABI_*`, API dispatcher y validación |
| `kernel/syscall/syscall.c` | Tabla, handlers stub, `metta_syscall_dispatch`, punteros usuario |
| `docs/swarm/sa-10-report.md` | Este informe |

**Modificados:** ninguno fuera de `kernel/syscall/`, `kernel/include/syscall/` y documentación swarm.

## Convención de llamada (x86_64)

| Registro | Uso |
|----------|-----|
| `rax` | Número `METTA_SYS_*` |
| `rdi` … `r9` | Seis argumentos |
| `rax` (retorno) | `≥ 0` éxito; `< 0` → `METTA_ABI_E*` |

Revisión ABI: `METTA_ABI_REV` = 1.

## Syscalls implementadas (handlers)

| NR | Símbolo | Comportamiento actual |
|----|---------|------------------------|
| 0 | `METTA_SYS_NOP` | Retorna 0 |
| 1 | `METTA_SYS_EXIT` | Stub (sin zombie SA-09) |
| 2 | `METTA_SYS_YIELD` | Stub (sin `sched_yield`) |
| 3 | `METTA_SYS_GETPID` | `metta_syscall_current_pid()` |
| 4 | `METTA_SYS_WRITE` | Valida buffer usuario; fd 1/2; retorna `len` |
| 5 | `METTA_SYS_READ` | Valida buffer; fd 0; retorna 0 |
| 0x100 | `METTA_SYS_MDE_HELLO` | Requiere contexto usuario + C-string válida |
| 0x101–0x104 | MDE ventana/FB | `METTA_ABI_ENOTSUP` |

Cualquier otro número registrado en init responde `METTA_ABI_ENOSYS`.

## Validación de punteros (stub)

- `metta_user_ptr_validate(ptr, len, access)`  
  NULL → `METTA_ABI_EINVAL`; fuera de `[METTA_USER_ADDR_MIN, METTA_USER_ADDR_MAX]` o no canónico bajo → `METTA_ABI_EFAULT`.  
  **Pendiente:** `vmm_probe_user_range` (SA-08) y permisos PTE (SA-09).

- `metta_user_cstr_validate(str, max_len)`  
  Recorre hasta `NUL` o `max_len`, mismas reglas de rango.

- Contexto: `metta_syscall_set_context(pid, is_user)` — **SA-09** debe llamarlo al saltar a ring 3; MDE syscalls exigen `is_user != 0`.

## Contrato con otros subsistemas

| Par | Obligación |
|-----|------------|
| **Process manager (SA-09)** | Antes de `SYSCALL` desde usuario: `metta_syscall_set_context(pid, 1)`; en kernel trap: `is_user=0`. Implementar `EXIT`/`YIELD` reales. |
| **VMM (SA-08)** | Sustituir stub de rango por comprobación de mapping usuario. |
| **MDE / WM (SA-15+)** | Usar banda `METTA_SYS_MDE_*`; reemplazar stubs `sys_mde_win_stub` por IPC al compositor. |
| **Linux ABI (futuro)** | Reservada base `0x200`; no mezclar con tabla nativa. |

## Integración

```c
/* kmain, tras arch_init (cuando exista vector SYSCALL) */
metta_syscall_init();

/* stub asm (SA-05): tras leer rax/di/... */
metta_syscall_dispatch((uint32_t)nr, a0, a1, a2, a3, a4, a5);
```

Añadir a toolchain (SA-03): `kernel/syscall/syscall.c`, `-Ikernel/include`.

## Pruebas

| Prueba | Resultado |
|--------|-----------|
| Compilación freestanding | **No ejecutada** — sin `Makefile` unificado en workspace |
| Unit tests punteros | **Manual / pendiente** SA-35 |

## Dependencias

| De | Para |
|----|------|
| SA-05/09 | Vector `SYSCALL`, `metta_syscall_set_context` |
| SA-08 | Validación PTE usuario |
| SA-03 | Enlace de `syscall.c` |

## Entrega estándar (7 puntos)

1. **Resumen:** dispatcher nativo + ABI v1 + validación punteros stub.  
2. **Archivos:** tabla arriba.  
3. **Ámbito:** solo syscall + informe.  
4. **Interfaces:** `metta_abi.h`; alineado con rangos usuario en `boot-map.md`.  
5. **Pruebas:** pendientes QEMU/build.  
6. **Errores:** ninguno lógico conocido en dispatcher.  
7. **Pendientes:** trap hardware, VMM probe, handlers reales proceso/MDE.

---

*Native Syscall Engineer — SA-10 completado.*
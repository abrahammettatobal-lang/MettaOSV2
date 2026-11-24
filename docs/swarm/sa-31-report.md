# SA-31 — Linux ABI dispatcher (x86_64)

| Campo | Valor |
|-------|-------|
| Misión | SA-31 Linux ABI syscalls |
| Agente | linux-abi-engineer |
| Estado | COMPLETED (cableado SA-32) |
| Fecha | 2026-11-22 |

## Resumen

Capa de dispatch para **syscalls Linux x86_64**: tabla indexada por `__NR_*`, manejadores que delegan en `linux_rt_*`. Tras **SA-32**, `syscall_bridge.c` llama a APIs reales del runtime donde existen; el resto sigue en stubs débiles **`-ENOSYS`**. NR sin entrada en tabla → **`-ENOSYS`**.

## Archivos creados (`kernel/linux/syscall*`)

| Archivo | Rol |
|---------|-----|
| `syscall_linux.h` | Marco de trap, API `linux_syscall_dispatch*`, contrato `linux_rt_*` |
| `syscall_linux.c` | Init, dispatch, contador ENOSYS, `linux_errno_from_metta` |
| `syscall_table.c` | Tabla 512 entradas + ~35 syscalls cableadas (I/O, memoria, señales, identidad) |
| `syscall_bridge.c` | Puente fuerte a SA-32 + gaps `weak` → `-ENOSYS` |
| `syscall_numbers.h` | Constantes `__NR_*` x86_64 (`LINUX_SYSCALL_MAX` = 512) |
| `syscall_errno.h` | Subconjunto errno Linux para el ABI |
| `docs/swarm/sa-31-report.md` | Este informe |

## API pública

```c
void linux_syscall_init(void);
int64_t linux_syscall_dispatch(const linux_syscall_frame_t *frame);
int64_t linux_syscall_dispatch_raw(uint64_t nr, uint64_t rdi, ...);
const linux_syscall_fn_t *linux_syscall_table_ref(size_t *count_out);
uint64_t linux_syscall_enosys_count(void);
int linux_errno_from_metta(int metta_status);
```

**Convención x86_64:** `rax=nr`, argumentos `rdi, rsi, rdx, r10, r8, r9`. Retorno: éxito ≥ 0 o `-(errno)`.

## Syscalls con entrada en tabla (delegan a `linux_rt_*`)

`read`, `write`, `open`, `close`, `fstat`, `lseek`, `mmap`, `mprotect`, `munmap`, `brk`, `rt_sigaction`, `rt_sigprocmask`, `rt_sigreturn`, `ioctl`, `pipe`, `nanosleep`, `getpid`, `exit`, `fcntl`, `dup`, `dup2`, `getcwd`, `chdir`, `unlink`, `readlink`, `uname`, `getuid`/`getgid`/`geteuid`/`getegid`, `arch_prctl`, `gettid`, `set_tid_address`, `clock_gettime`, `exit_group`, `getdents64`, `openat`, `newfstatat`.

**Resto de NR (0..511):** puntero `NULL` en tabla → `-ENOSYS` sin llamar a runtime.

## Integración MettaOS ↔ Linux (post SA-32)

| `linux_rt_*` | Backend SA-32 |
|--------------|----------------|
| `read` / `write` | `linux_pipe_read` / `linux_pipe_write`; TTY stub (read 0, write acepta `count`) |
| `open` / `openat` | `linux_fd_open_stub(..., LINUX_FD_FILE_STUB)` |
| `close` | `linux_fd_close` |
| `pipe` | `linux_pipe_create` |
| `getpid` / `gettid` | `linux_proc_pid(linux_proc_current())` |
| `exit` / `exit_group` | `linux_proc_mark_zombie` |
| `rt_sigaction` / `rt_sigprocmask` | `linux_sigaction` / `linux_sigprocmask` (`sigsetsize` = 8) |
| mmap, brk, fstat, dup, uname, clock, … | Sin símbolo SA-32 → stub débil **`-ENOSYS`** |

`linux_syscall_init()` invoca `linux_runtime_init()` (proceso init + stdio TTY stub).

| Pendiente | Nota |
|-----------|------|
| Trap handler | `linux_syscall_frame_t` → `linux_syscall_dispatch` |
| VFS / MM | `open` con path real, `mmap`, `brk` |
| `kill` en tabla | Usar `linux_kill` cuando se amplíe `syscall_table.c` |

## Compilación local

```bash
gcc -std=gnu11 -ffreestanding ... -Ikernel/include \
  -c kernel/linux/syscall_linux.c \
     kernel/linux/syscall_table.c \
     kernel/linux/syscall_bridge.c
```

GCC resuelve `#include "syscall_linux.h"` desde el directorio del `.c` (`kernel/linux/`).

El `Makefile` (SA-03) ya incluye `kernel/*/*.c` → estos objetos entran en el enlace automáticamente.

## Prueba ejecutada

```bash
wsl bash -lc 'cd /mnt/c/Users/USER/Desktop/MettaOS && gcc ... (tres .c) ... && echo COMPILE_OK'
```

| Resultado | Nota |
|-----------|------|
| No ejecutada | WSL sin `gcc` en PATH (`INFRA_FAIL`, igual que SA-35) |

Tras `scripts/wsl-setup.sh`, repetir compilación de los tres `.c` anteriores.

## Limitaciones

- Sin validación de punteros usuario (pendiente VMM + runtime SA-32).
- Sin cableado desde IDT/`syscall` insn (misma oleada Linux).
- Syscalls de red, `clone`/`execve`, epoll, etc.: solo ENOSYS hasta ampliar tabla.

## Dependencias

| De | Para |
|----|------|
| SA-32 Linux runtime | **Integrado** en `syscall_bridge.c` |
| SA-05/06 CPU traps | Entregar frames al dispatcher |
| SA-13 VFS | Backing real de `open`/`read`/… |

## Conclusión

Dispatcher, tabla y puente **cableados a SA-32** para I/O pipe/TTY, fds, procesos y señales stub; gaps (VFS, memoria, identidad) permanecen **ENOSYS**.
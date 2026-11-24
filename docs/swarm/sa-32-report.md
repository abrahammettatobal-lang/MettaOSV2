# SA-32 — Linux runtime (procesos, fds, pipes, señales stub)

| Campo | Valor |
|-------|-------|
| Misión | SA-32 Linux runtime |
| Agente | linux-runtime-engineer |
| Estado | COMPLETED (stubs enlazables) |
| Fecha | 2026-11-22 |

## Entregables

| Archivo | Descripción |
|---------|-------------|
| `kernel/linux/runtime.c` | Tabla de procesos, fds, pipes en anillo 4 KiB |
| `kernel/linux/signal.c` | Máscaras, `kill`, `sigaction` / `sigprocmask` stub |
| `kernel/include/linux/runtime.h` | Contrato procesos, errno negativo, API pipe/fd |
| `kernel/include/linux/signal.h` | Números SIG, `linux_signal_state_t` |
| `docs/swarm/sa-32-report.md` | Este informe |

## Comportamiento implementado (stub)

### Procesos

- Hasta **32** procesos; PID monotónico desde 1.
- Proceso **init** en `linux_runtime_init()`: stdin/stdout/stderr como `LINUX_FD_TTY_STUB`.
- Estados: `RUNNABLE`, `ZOMBIE`, `DEAD`; `linux_proc_spawn_stub`, `linux_proc_reap`.
- Sin fork real, sin context switch ni cargador ELF (pendiente scheduler + SA ELF).

### Descriptores y pipes

- Hasta **256** fds por proceso; tipos `TTY_STUB`, `FILE_STUB`, `PIPE_RD` / `PIPE_WR`.
- `linux_pipe_create`: par enlazado; buffer circular **4096** bytes (`METTA_LINUX_PIPE_BUF`).
- `linux_pipe_read` / `linux_pipe_write`: retorno POSIX-like (`ssize_t` positivo o errno negativo).
- Cierre de extremo escritor con lector abierto → `LINUX_SIGPIPE` vía `linux_kill` + `EPIPE` en write.

### Señales

- Conjunto de **64** señales; bitsets `pending` / `blocked`.
- `linux_kill`, `linux_sigaction`, `linux_sigprocmask`, `linux_signal_deliver_pending` (consume pending; **sin** trampoline a userspace).
- `SIGKILL` en entrega marca proceso zombie con código `128 + sig`.

## Convenciones ABI

- Códigos de error de syscall: **negación** de `linux_errno_t` (`LINUX_EINVAL`, `LINUX_EBADF`, …).
- Kernel nativo sigue usando `metta_status_t` en APIs `metta_status_t` del runtime.

## Fuera de alcance (explícito)

- Dispatcher `syscall` Linux → MettaOS (`linux-abi-engineer`).
- Señales entregadas en stack de userspace, `rt_sigreturn`, multiproceso real.
- Pipes entre procesos distintos, `select`/`poll`, epoll.
- Integración VMM / duplicación de tabla fd en fork.

## Dependencias

| Dependencia | Nota |
|-------------|------|
| SA-03 Makefile | `kernel/*/*.c` ya entra en `KERNEL_SRCS` |
| SA-17 scheduler | Context switch y proceso actual real |
| Linux ABI SA futura | Traducir `pipe`, `kill`, `sigaction` desde trap |

## Verificación sugerida

```bash
# Con toolchain WSL (SA-02):
make all
# Debe compilar runtime.c y signal.c sin enlazar kmain a linux_runtime_init.
```

Prueba manual futura: llamar `linux_runtime_init()`, `linux_pipe_create`, round-trip write/read, `linux_kill(SIGINT)`, `linux_signal_deliver_pending`.

## Próximos pasos

1. Registrar núcleo Linux en `kmain` tras PMM/IRQ cuando exista userspace de prueba.
2. Cablear syscalls `pipe`, `close`, `kill`, `rt_sigaction` en capa ABI.
3. Fork: clonar tabla fd y refcount pipes; entrega de señales con trampolín SIG.
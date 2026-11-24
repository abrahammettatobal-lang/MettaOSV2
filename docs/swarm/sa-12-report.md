# SA-12 — Panic, printk ampliado y notas GDB/QEMU

**Agente:** debugging-diagnostics-engineer  
**Estado:** COMPLETED  
**Complementa:** SA-06 (UART COM1, `printk` base)

## Entregables

| Ruta | Rol |
|------|-----|
| `kernel/panic.c` | `panic`, `panic_fmt`, `panic_halt` (serial + halt) |
| `kernel/log/printk.c` | `vprintk`, niveles `KLOG_*`, filtros por nivel |
| `kernel/include/log/panic.h` | API `panic` (noreturn) |
| `kernel/include/log/klog.h` | Macros `KLOG_ERR/WARN/INFO/DEBUG`, `KLOG_BOOT` |
| `kernel/include/log/printk.h` | Declaración `vprintk` |
| `docs/swarm/qemu-gdb-stub-notes.md` | QEMU `-S -s` + GDB remoto |
| `docs/swarm/sa-12-report.md` | Este informe |

## API pública

### Panic

```c
void panic(const char *msg);           /* noreturn */
void panic_fmt(const char *fmt, ...);  /* noreturn */
void panic_halt(void);                 /* noreturn — cli; hlt */
```

Salida serie (ejemplo):

```text
*** METTAOS PANIC ***
panic: boot handoff invalid
```

### KLOG (sobre COM1)

```c
void klog_set_level(klog_level_t level);
klog_level_t klog_get_level(void);

KLOG_ERR("pmm corrupt frame %p\n", (void *)frame);
KLOG_WARN("retry %d\n", n);
KLOG_INFO("vmm ready\n");
KLOG_DEBUG("pte %llx\n", (unsigned long long)pte);
KLOG_BOOT("MDE hold %u ms\n", hold_ms);
```

Nivel por defecto: `KLOG_DEFAULT_LEVEL` → `KLOG_LEVEL_INFO` (sobreescribible en compilación).

### printk ampliado

Especificadores añadidos respecto a SA-06: `%p`, `%lld`, `%llx`, `%ld`, `%lu`, `%lx`.

```c
void printk(const char *fmt, ...);
void vprintk(const char *fmt, va_list args);
```

## Secuencia en `kmain`

Ver **`docs/swarm/kmain-bringup.md`** (orden completo: validate → serial → `cpu_init` → PMM/VMM → security → scheduler opcional → PIC/PIT/`sti` → `KLOG_BOOT` / FB).

1. `serial_init()` (SA-06) — **obligatorio** antes de log o panic.
2. Tras IRQ: `KLOG_BOOT(...)`; fallos PMM/VMM → `panic_fmt`.
3. En fallo irrecuperable: `panic("...")` o `panic_fmt("code %d", code)` (p. ej. `boot-map.md`, PMM/VMM).

## Prueba en QEMU (serie)

```text
qemu-system-x86_64 -m 4096 -cdrom build/mettaos.iso -serial stdio -display none
```

Invocar `panic("test")` desde un punto de prueba en `kmain` debe mostrar el banner y detener ticks PIT visibles.

## Depuración con GDB

Ver `docs/swarm/qemu-gdb-stub-notes.md` (`-S -s`, `target remote :1234`, breakpoints en `panic` / `kmain`).

## Compilación unitaria

```bash
INC="-Ikernel/include"
FLAGS="-ffreestanding -fno-stack-protector -mno-red-zone -mcmodel=kernel -Wall -Wextra -c"

x86_64-elf-gcc $FLAGS $INC -o printk.o kernel/log/printk.c
x86_64-elf-gcc $FLAGS $INC -o panic.o  kernel/panic.c
```

El `Makefile` (SA-03) recoge `kernel/panic.c` vía `kernel/*.c`.

## Dependencias

| Entrada | Notas |
|---------|--------|
| SA-06 | `serial_init`, `serial_putc`, `serial_write_str` |
| SA-05 (futuro) | `#DF` / page fault podrán llamar `panic_fmt` con RIP desde handler |

| Salida | Consumidor |
|--------|------------|
| `panic*` | PMM, VMM, boot handoff (SA-04/07/09), SA-11 policy |
| `KLOG_BOOT` | SA-21 bootsplash cuando exista `bootsplash.c` |

## Notas

- `panic_halt` no reinicia; en QEMU la VM queda congelada salvo GDB `continue` desde un breakpoint previo.
- Sin reentrancia en panic: no llamar `panic` desde dentro de `panic`.
- IRQ: panic no envía EOI; asumir fallo fatal con interrupciones deshabilitadas.
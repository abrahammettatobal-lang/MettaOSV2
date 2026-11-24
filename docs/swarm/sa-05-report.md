# SA-05 — GDT, TSS, IDT y excepciones mínimas

Estado: **COMPLETED** (fuentes; integración `kmain` queda a SA-04/integración).

## Entregables

| Ruta | Rol |
|------|-----|
| `kernel/cpu/cpu.c` | `cpu_init()` / `arch_init_idt_gdt_tss()` |
| `kernel/cpu/gdt.c` | GDT 64-bit kernel + usuario (ring 3) + descriptor TSS |
| `kernel/cpu/tss.c` | TSS con `RSP0`, `IST1` (#DF), stub selectores Ring 3 |
| `kernel/cpu/idt.c` | 256 entradas; IRQ `0x20`–`0x2F` |
| `kernel/cpu/idt_stubs.c` | Stubs naked excepciones + IRQ |
| `kernel/cpu/exceptions.c` | `exception_handler` → `printk` + `kernel_halt` |
| `kernel/include/cpu/*.h` | API pública CPU |

## Orden en `kmain` (boot-map.md)

Tras `vmm_late_init_after_pmm()` y **antes** de `pic_init` / `sti`:

1. `serial_init()` (SA-06)
2. `cpu_init()` — o `arch_init_idt_gdt_tss()` (alias)
3. `pic_init()`, `irq_init()`, `pit_init()`
4. `sti()`

## GDT (post-`cpu_init`)

| Selector | Uso |
|----------|-----|
| `0x08` | Código kernel 64-bit |
| `0x10` | Datos kernel |
| `0x1B` | Código usuario (DPL 3), stub |
| `0x23` | Datos usuario (DPL 3), stub |
| `0x28` | TSS 64-bit (16 B descriptor) |

El arranque en `boot/multiboot2.S` carga una GDT mínima; `lgdt` en `gdt_init()` sustituye la tabla del kernel.

## TSS

- `RSP0`: stack dedicado 16 KiB (entradas ring 0 desde ring 3 / IRQ con cambio de privilegio).
- `IST1`: stack 4 KiB para vector **8** (`#DF`).
- `iopb_offset` = tamaño TSS (sin mapa I/O).
- `tss_user_code_selector()` / `tss_user_data_selector()` exponen `0x1B` / `0x23` para scheduler/VMM.

## IDT

| Rango | Comportamiento |
|-------|----------------|
| `0x00`–`0x1F` | Excepciones → `exception_handler` + halt |
| `0x08` | Gate con **IST=1** |
| `0x20` | `irq0_entry()` (contrato SA-06 / PIT) |
| `0x21`–`0x2F` | `pic_send_eoi(irq)` sin handler registrado |

## API pública

```c
void cpu_init(void);
void arch_init_idt_gdt_tss(void);

void gdt_init(void);
void gdt_install_tss_descriptor(void);

void tss_prepare(void);
void tss_load_task_register(void);
tss64_t *tss_get(void);
uint16_t tss_user_code_selector(void);
uint16_t tss_user_data_selector(void);

void idt_init(void);
void idt_set_gate(uint8_t vector, void (*handler)(void), uint8_t type_attr, uint8_t ist);
```

## Compilación

Los objetos entran vía `KERNEL_SRCS` en el Makefile (wildcard `kernel/*/*.c`).

```bash
make print-objs   # debe listar kernel/cpu/*.o
```

## Límites respetados

- Sin cambios en `boot/`, `kernel/kernel.c`, ni scheduler.
- Stubs en C (`naked`) dentro de `kernel/cpu/` (sin `.S` adicional).

## Dependencias

- **Entrada:** `kernel/log/printk.h`, `kernel.h`, `interrupt/irq.h`, `interrupt/pic.h` (solo vector 0x20+).
- **Consumidores:** integración `kmain`, pruebas QEMU SA-35.

## Próximo paso

Enlazar `cpu_init()` en `kmain` tras VMM/serial; con `sti`, validar `[tick] N s` en serie (SA-06).
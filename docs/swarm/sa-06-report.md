# SA-06 — IRQ, PIT y diagnóstico serie

Estado: **COMPLETED** (fuentes listas; integración con IDT/`kmain` en SA-04/SA-05).

## Entregables

| Ruta | Rol |
|------|-----|
| `kernel/serial.c`, `kernel/include/serial.h` | UART 16550 COM1 `0x3F8`, 115200 8N1 |
| `kernel/interrupt/pic.c` | Remap 8259 → IRQ `0x20`–`0x2F` |
| `kernel/interrupt/pit.c` | PIT canal 0 a 100 Hz, contador y `pit_sleep_ms` |
| `kernel/interrupt/irq.c` | Despacho IRQ0 → `pit_on_irq` + EOI |
| `kernel/include/interrupt/*.h` | Puertos I/O, PIC, PIT, IRQ |
| `kernel/log/printk.c` | `printk` mínimo sobre serial |

## Secuencia esperada en `kmain` (post-SA-04)

Orden obligatorio antes de `sti()`:

1. `serial_init()`
2. `printk("MettaOS early console\n")` (opcional)
3. `pic_init()`
4. `irq_init()` — registra manejador del timer
5. `pit_init()` — desenmascara IRQ0
6. Tras instalar vector **0x20** en la IDT apuntando al stub que llama `irq0_entry()`, habilitar interrupciones (`sti`)

### Contrato con IDT (SA-05)

- Vector de hardware **0x20** = IRQ0 (timer PIT) tras remap del PIC.
- El stub de interrupción debe preservar registros y llamar a `void irq0_entry(void);` (definido en `irq.c`).

## API pública

### Serial (`serial.h`)

```c
void serial_init(void);
void serial_putc(char c);
void serial_write(const char *buf, size_t len);
void serial_write_str(const char *s);
```

### PIC / PIT / IRQ

```c
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

void pit_init(void);
void pit_on_irq(void);      /* solo vía irq_dispatch */
uint64_t pit_ticks_get(void);
void pit_sleep_ms(uint32_t ms);

void irq_init(void);
void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_dispatch(uint8_t irq);
void irq0_entry(void);      /* entrada desde ensamblador */
```

### Log

```c
void printk(const char *fmt, ...);
```

Especificadores: `%`, `%c`, `%s`, `%d`, `%u`, `%x`, `%X`, `%llu`, `%lu`.

## Prueba en QEMU (cuando exista imagen SA-03/SA-04)

```text
qemu-system-x86_64 -kernel build/mettaos.kernel -serial stdio -display none
```

Tras `sti`, la consola serie debe mostrar cada segundo:

```text
[tick] 1 s
[tick] 2 s
...
```

(El PIT corre a **100 Hz**; el mensaje se emite una vez por segundo.)

## Compilación unitaria (sin Makefile raíz aún)

Desde la raíz del repo, con `x86_64-elf-gcc` o equivalente freestanding:

```bash
INC="-Ikernel/include"
FLAGS="-ffreestanding -fno-stack-protector -mno-red-zone -mcmodel=kernel -Wall -Wextra -c"

x86_64-elf-gcc $FLAGS $INC -o serial.o  kernel/serial.c
x86_64-elf-gcc $FLAGS $INC -o pic.o     kernel/interrupt/pic.c
x86_64-elf-gcc $FLAGS $INC -o pit.o     kernel/interrupt/pit.c
x86_64-elf-gcc $FLAGS $INC -o irq.o     kernel/interrupt/irq.c
x86_64-elf-gcc $FLAGS $INC -o printk.o  kernel/log/printk.c
```

En WSL/Ubuntu de MettaOS (`scripts/wsl-setup.sh`), instalar `gcc-x86-64-elf` si hace falta.

## Dependencias

- **Entrada:** ninguna fuente en `boot/` ni `mde/`.
- **Salida:** SA-05 (IDT vector 0x20), SA-03 (Makefile enlaza objetos), SA-35 (harness QEMU).

## Notas

- `pit_sleep_ms` usa `hlt`; requiere interrupciones habilitadas y PIC desenmascarado.
- HPET/APIC tick queda fuera de alcance (stub futuro).
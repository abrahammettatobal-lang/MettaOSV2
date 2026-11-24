# QEMU GDB stub — notas de depuración (SA-12)

Diagnóstico del kernel en **serial COM1** (SA-06) y parada controlada con **GDB** vía stub de QEMU.

## Arranque con stub

El Makefile usa `-serial stdio` (ver `QEMU_FLAGS`). Para depurar añadir `-S` (pausa al arrancar) y `-s` (stub en TCP 1234):

```text
qemu-system-x86_64 -m 4096 -cdrom build/mettaos.iso \
  -serial stdio -display none \
  -S -s
```

| Flag | Efecto |
|------|--------|
| `-S` | CPU en halt hasta que GDB continúe (`continue`) |
| `-s` | Stub GDB en `localhost:1234` (equivale a `-gdb tcp::1234`) |

En WSL, el puerto 1234 del host suele ser alcanzable como `localhost:1234` desde la misma máquina.

## Sesión GDB (toolchain freestanding)

Con `gdb-multiarch` o `x86_64-elf-gdb`:

```text
x86_64-elf-gdb build/mettaos.bin
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue
```

Si los símbolos no resuelven, cargar explícitamente:

```text
(gdb) file build/mettaos.bin
(gdb) set disassembly-flavor intel
```

## Panic y punto de parada

Tras `panic()` / `panic_fmt()` (SA-12) la CPU entra en `cli` + bucle `hlt` (`panic_halt`). En GDB:

```text
(gdb) break panic
(gdb) break panic_halt
```

La consola **serial** (stdio) muestra el banner `*** METTAOS PANIC ***` y el mensaje antes del halt — útil si GDB no está conectado.

## Macros KLOG y printk

- `KLOG_*` y `printk` salen por COM1; mantener `-serial stdio` al depurar.
- `KLOG_DEBUG` se filtra con `klog_set_level()`; en depuración profunda subir a `KLOG_LEVEL_DEBUG`.

## Limitaciones actuales

- Sin stub in-kernel propio: solo stub de QEMU.
- IDT/excepciones (SA-05): breakpoints en vectores requieren stubs instalados.
- ISA Multiboot/GRUB: direcciones en GDB son **físicas/linkeadas** según `linker.ld`; HHDM pendiente de VMM.

## Relación con SA-06 / SA-35

- **SA-06:** `serial_init()` antes de cualquier `printk` / `KLOG_*` / `panic`.
- **SA-35:** el harness smoke puede extenderse con objetivo `qemu-gdb` documentado aquí; no forma parte del build por defecto.
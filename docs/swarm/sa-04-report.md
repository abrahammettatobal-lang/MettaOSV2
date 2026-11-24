# SA-04 — Boot & Long Mode

**Misión:** arranque Multiboot2 → Long Mode → handoff `metta_boot_info_t` → `kmain`.  
**Estado:** COMPLETED (fuentes); alineado a **contrato SA-01 rev 1**.  
**Enlace / QEMU:** SA-03.

## Cambio de contrato (post SA-01)

| Antes (proto SA-04) | Ahora (contrato oficial §5) |
|---------------------|---------------------------|
| `kmain(uint64_t magic, void *mbi)` | `kmain(metta_boot_info_t *info)` **`noreturn`** |
| Magic Multiboot2 crudo en `kmain` | `METTA_BOOT_MAGIC` (`0x4D455454415F424F`) en `info->magic` |
| Sin validación | `boot_handoff_validate()` obligatorio antes de `pmm_init` |
| — | `contract_rev == METTA_CONTRACT_REV` (1) |
| — | `memmap`, FB y límites kernel en `metta_boot_info_t` |

**Trampolín:** el stub 64-bit llama `boot_enter(mb2_magic, mbi)` → `boot_build_info_from_mbi` → `kmain(&g_boot_info)`.

## Entregables

| Ruta | Rol |
|------|-----|
| `boot/multiboot2.S` | MB2, Long Mode, `call boot_enter` |
| `boot/longmode.S` | `setup_identity_map` (2 MiB identidad) |
| `kernel/boot/handoff.c` | `boot_build_info_from_mbi`, `boot_handoff_validate`, `boot_enter` |
| `kernel/boot/handoff.h` | API del trampolín |
| `kernel/include/metta/types.h` | Tipos contrato rev 1 |
| `kernel/include/metta/boot_info.h` | `metta_boot_info_t` |
| `kernel/kernel.c` | `kmain`: validate → halt (PMM SA-07) |

## Flujo de arranque

1. GRUB → `_start` (32-bit); guardar magic/MBI.
2. GDT, PAE, identidad 2 MiB, Long Mode.
3. `boot_enter(EAX, EBX)` rellena `g_boot_info` desde tags mmap / FB / load_base.
4. `kmain` ejecuta `boot_handoff_validate`; fallo → `kernel_halt` (panic serial en SA-06).

## Verificación local

```bash
nasm -f elf64 boot/multiboot2.S -o boot/multiboot2.o
nasm -f elf64 boot/longmode.S    -o boot/longmode.o
gcc -ffreestanding -fno-builtin -fno-stack-protector -nostdlib \
    -c kernel/kernel.c -I kernel/include -o kernel/kernel.o
gcc -ffreestanding -fno-builtin -fno-stack-protector -nostdlib \
    -c kernel/boot/handoff.c -I kernel/include -I kernel/boot -o kernel/boot/handoff.o
```

## Símbolos

| Símbolo | Rol |
|---------|-----|
| `boot_enter` | Entrada C desde ensamblador |
| `boot_build_info_from_mbi` | Parse MB2 → `metta_boot_info_t` |
| `boot_handoff_validate` | Comprueba magic, rev, memmap, FB, kernel bounds |
| `__kernel_start` / `__kernel_end` | Stubs débiles; **SA-03** linker provee reales |

## Límites

- Sin PMM, VMM, Makefile ni MDE.
- Path Long Mode sin reescritura (solo destino de `call`).

## Próximo

- **SA-03:** símbolos de imagen y enlace.
- **SA-07:** `pmm_init(info->memmap, info->memmap_count)` tras validate.
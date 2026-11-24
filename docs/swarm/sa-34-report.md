# SA-34 — Linux GUI / puente X11 y embed MDE

| Campo | Valor |
|-------|-------|
| Misión | AF_UNIX stub, sin ventanas falsas |
| Agente | linux-gui-compatibility-engineer |
| Estado | COMPLETED (stubs iniciales) |
| Fecha | 2026-11-22 |

## Entregables

| Archivo | Descripción |
|---------|-------------|
| `kernel/linux/x11_bridge.c` | Stub de puente X11 sobre sockets **AF_UNIX**; ventanas solo tras opcodes X11 del cliente |
| `mde/linux/embed.c` | Capa MDE que **no** crea shells hasta `x11_bridge_window_query` devuelva XID mapeado |
| `docs/swarm/sa-34-report.md` | Este informe |

## Política «sin ventanas falsas»

1. `x11_bridge_init` no precarga ningún XID ni superficie.
2. `x11_bridge_af_unix_accept` solo abre una ranura de conexión; no declara ventanas.
3. Los XIDs entran al registro únicamente con tráfico parseado (`CreateWindow` / `MapWindow` / `DestroyWindow` mínimo).
4. `mde_linux_embed_attach` falla con `MDE_EMBED_ERR_NOT_FOUND` si no hay ventana **mapeada** en el puente.
5. `mde_surface_id` permanece `0` hasta que el compositor (misión futura) asigne una superficie real.

## API kernel (`x11_bridge.c`)

| Símbolo | Rol |
|---------|-----|
| `x11_bridge_init` / `x11_bridge_shutdown` | Ciclo de vida |
| `x11_bridge_ready` | Listo tras `init` |
| `x11_bridge_af_unix_accept` | `LINUX_AF_UNIX` + ruta tipo `.X11-unix/X*` |
| `x11_bridge_af_unix_close` | Cierra conexión y libera ventanas asociadas |
| `x11_bridge_feed` | Bytes X11 del cliente; parser stub |
| `x11_bridge_window_query` | Consulta para MDE (solo ventanas mapeadas) |
| `x11_bridge_window_count` | Contador de ventanas mapeadas |

## API MDE (`embed.c`)

| Símbolo | Rol |
|---------|-----|
| `mde_linux_embed_init` | Reset de ranuras |
| `mde_linux_embed_attach` | Enlaza pid + XID si el puente confirma geometría |
| `mde_linux_embed_sync_geometry` | Refresca tamaño o elimina embed si desaparece |
| `mde_linux_embed_detach` / `_all_for_pid` | Limpieza |
| `mde_linux_embed_active_count` | Métrica de embeds activos |

## Dependencias

| Dep | Nota |
|-----|------|
| SA-32 `kernel/linux/runtime.c` | `linux_pid_t` y runtime listo |
| SA-15 compositor | Asignación de `mde_surface_id` y blit framebuffer |
| SA-33 dispatcher Linux | `connect`/`sendmsg` AF_UNIX hacia el puente |
| `Makefile` | `kernel/linux/x11_bridge.c` entra vía `kernel/*/*.c` |

## Fuera de alcance (explícito)

- Headers públicos en `kernel/include/linux/x11_bridge.h` (contrato embebido en `.c` por alcance de misión).
- Servidor X11 completo, SHM, GLX, extensiones.
- Ventanas MDE decoradas sin pixmap real del cliente.
- Integración en `kmain` o ciclo del compositor.

## Pruebas sugeridas

1. Compilar el kernel con toolchain WSL (`make all`) y confirmar enlace de `x11_bridge.o`.
2. Unit harness host: simular `CreateWindow` + `MapWindow` en `x11_bridge_feed` y comprobar `window_query`.
3. `mde_linux_embed_attach` con XID inexistente → `MDE_EMBED_ERR_NOT_FOUND`.
4. Tras `DestroyWindow` en el feed, `sync_geometry` debe liberar el embed.

## Conclusión

Stub AF_UNIX + embed MDE alineados con la política **sin ventanas falsas**; listos para cablear al dispatcher Linux y al compositor en misiones posteriores.
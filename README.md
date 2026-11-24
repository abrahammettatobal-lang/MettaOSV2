# MettaOS

Sistema operativo en desarrollo. Este repositorio sigue el plan de misiones SA (swarm).

## Desarrollo (Windows 11 + WSL2)

1. Instalar [WSL2 con Ubuntu](https://learn.microsoft.com/windows/wsl/install).
2. Instalar toolchain (elige una vía):

   ```bash
   # Ubuntu WSL (interactivo; pedirá contraseña sudo)
   chmod +x scripts/wsl-setup.sh && ./scripts/wsl-setup.sh
   ```

   ```powershell
   # PowerShell si sudo no es sin contraseña en agentes/CI
   .\scripts\win-dev.ps1 -RunSetup
   # o: wsl -u root bash scripts/wsl-setup.sh
   ```

3. Desde PowerShell en Windows:

   ```powershell
   .\scripts\win-dev.ps1 -VerifyOnly    # solo comprueba WSL y rutas
   .\scripts\win-dev.ps1                # ejecuta `make` en WSL
   ```

### Dependencias instaladas por `wsl-setup.sh`

`gcc`, `nasm`, `make`, `grub-pc-bin`, `xorriso`, paquete QEMU (`qemu-system-x86` en Ubuntu 24.04+/26.04; binario `qemu-system-x86_64`) y `build-essential`.

Reporte de la misión SA-02: [docs/swarm/sa-02-report.md](docs/swarm/sa-02-report.md).
## Red y disco de MDE en QEMU

Desde Windows, compila con WSL y arranca con QEMU para Windows.

```powershell
.\scripts\win-dev.ps1 -WslCommand "make -j4 iso"
.\scripts\run-qemu.ps1
```

También puedes usar `make run` dentro de WSL. Ambos lanzadores reutilizan el archivo
`data/mettaos.img` del proyecto. Si el AppImage de Firefox está en Descargas,
el primer arranque crea un disco de unos 128 MiB y deja el archivo en
`/home/usuario/Documentos/Firefox.AppImage` (el mismo inodo que
`/home/usuario/firefox`). Sin ese AppImage, el disco vacío sigue siendo de 32 MiB.
MFS ocupa el volumen a partir del sector 8. QEMU arranca con 6 GiB de RAM.
El archivo permanece al recompilar o limpiar la ISO. Cierra QEMU antes
de copiar el disco para una copia de seguridad.

En MDE, Archivos, Notas y Terminal guardan los cambios en ese volumen. El escritorio
alterna dos catálogos con suma de comprobación y actualiza los datos modificados antes
de publicar el nuevo catálogo. El comando `disco` fuerza el guardado y muestra el estado.
Si arrancas solo la ISO, el escritorio indica **Temporal (RAM)** en Acerca de.

Elige la red Wi-Fi en Windows. El menú superior de MDE abre **Ajustes → Red**.
El perfil de QEMU usa una RTL8139, IPv4 estática 10.0.2.15, puerta de enlace
10.0.2.2 y DNS 10.0.2.3. `red` muestra el estado confirmado por ARP.
`dns example.com` envía una consulta IPv4 y `dns` muestra el resultado.
El lanzador de Windows también consulta el resolvedor IPv4 del equipo por el
segundo puerto serie virtual. MettaOS usa la primera dirección válida que recibe.

En Terminal, `ping 10.0.2.2` comprueba la puerta de enlace de QEMU. Usa
`ping 1.1.1.1` para probar un destino público o `ping example.com` para probar
también DNS. Cada ejecución envía una solicitud ICMP y muestra el tiempo de
ida y vuelta. La espera de la respuesta ICMP dura hasta tres segundos.

Para arrancar la ISO verificada desde la raíz del proyecto:

```powershell
.\scripts\run-qemu.ps1 -Iso .\build\mettaos-mde.iso
```

Para repetir la prueba con un disco temporal:

```powershell
python tests/smoke-desktop.py --iso build/mettaos-mde.iso
```

MDE admite actualmente 160 entradas, hasta 64 archivos de texto y 8191 bytes de
contenido por archivo. La prueba de compatibilidad de binarios Linux está en la
segunda entrada de GRUB. El arranque normal entra directamente al escritorio.

## Escritorio local

`make run` y `.\scripts\run-qemu.ps1` arrancan solo MettaOS. El AppImage de
Firefox está en el disco MFS (`data/mettaos.img`), en Documentos. No se levanta
otra máquina virtual para ejecutarlo.

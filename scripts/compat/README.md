# GNU Bash y AppImage en MDE

El perfil opcional de QEMU conserva el kernel de MettaOS, MFS y el escritorio MDE. Una segunda máquina virtual Linux ejecuta las aplicaciones. Las ventanas se dibujan en MDE y reciben el teclado y el ratón del escritorio.

Desde la raíz del proyecto, configura una ISO Debian Live x86_64 que incluya Xvfb, xterm, Python 3, dbus-run-session, libX11, libXtst y las bibliotecas de las aplicaciones. La configuración guarda las rutas originales.

```powershell
.\scripts\run-qemu.ps1 -Prepare -LinuxIso D:\ISO\linux-live.iso -AppImage @('D:\Apps\Calculadora.AppImage','D:\Apps\Firefox.AppImage')
.\scripts\run-qemu.ps1 -Compat
```

En el perfil de compatibilidad, la terminal del dock abre GNU Bash. También puedes buscar «GNU Bash» con Ctrl+Espacio. Los AppImage importados aparecen en Archivos → Aplicaciones y en la búsqueda. Bash los ve en `/opt/AppImages`. Para incorporar paquetes nuevos al siguiente arranque, colócalos en `data/AppImages/`. Desde la consola nativa, `appimage /ruta/al/archivo.AppImage` permite abrir un paquete almacenado en MFS.

La carpeta personal de Bash se conserva en `data/linux-home.img`. El sistema nativo guarda sus propios archivos en `data/mettaos.img`. El escritorio de cada aplicación Linux usa X11 a 800 × 480 y renderizado por software. El cargador acepta paquetes x86_64 de tipo 1 y 2. Los de tipo 1 requieren bsdtar o xorriso. El funcionamiento de cada aplicación depende de las bibliotecas y los servicios presentes en la ISO. El perfil carece de audio, aceleración gráfica y compositor Wayland.

La ISO de MettaOS arrancada por sí sola ofrece el escritorio y la consola nativos. El perfil opcional requiere el lanzador de QEMU.

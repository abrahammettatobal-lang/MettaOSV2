#!/usr/bin/env bash
# MettaOS — dependencias de compilación en Ubuntu WSL (SA-02)
set -euo pipefail

# Ubuntu 24.04+: paquete qemu-system-x86 (binario sigue siendo qemu-system-x86_64).
QEMU_APT_PKG="qemu-system-x86"
if ! apt-cache show "$QEMU_APT_PKG" &>/dev/null 2>&1; then
  QEMU_APT_PKG="qemu-system-x86_64"
fi

METTAOS_PACKAGES=(
  build-essential
  gcc
  nasm
  make
  grub-pc-bin
  xorriso
  "$QEMU_APT_PKG"
)

log() { printf '[wsl-setup] %s\n' "$*"; }
die() { log "ERROR: $*"; exit 1; }

if [[ -f /proc/version ]] && grep -qi microsoft /proc/version; then
  log "Entorno WSL detectado."
else
  log "ADVERTENCIA: no parece WSL; continuando igualmente (útil en Ubuntu nativo)."
fi

if ! command -v apt-get >/dev/null 2>&1; then
  die "apt-get no disponible; use Ubuntu o Debian en WSL."
fi

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  SUDO=(sudo)
  if ! sudo -n true 2>/dev/null; then
    if [[ ! -t 0 ]]; then
      log "sudo requiere contraseña y esta sesión no es interactiva."
      log "Opción A — terminal Ubuntu: chmod +x scripts/wsl-setup.sh && ./scripts/wsl-setup.sh"
      log "Opción B — PowerShell: wsl -u root bash scripts/wsl-setup.sh"
      log "Opción C — PowerShell: .\\scripts\\win-dev.ps1 -RunSetup"
      die "Instale dependencias con una opción manual."
    fi
    log "Se solicitará contraseña sudo…"
  fi
else
  SUDO=()
fi

log "Actualizando índice de paquetes…"
"${SUDO[@]}" apt-get update -qq

log "Instalando: ${METTAOS_PACKAGES[*]}"
DEBIAN_FRONTEND=noninteractive "${SUDO[@]}" apt-get install -y --no-install-recommends "${METTAOS_PACKAGES[@]}"

log "Comprobando herramientas…"
MISSING=()
for tool in gcc nasm make grub-mkrescue xorriso qemu-system-x86_64; do
  if command -v "$tool" >/dev/null 2>&1; then
    log "  OK  $(command -v "$tool")"
  else
    MISSING+=("$tool")
  fi
done

if ((${#MISSING[@]} > 0)); then
  die "Faltan en PATH: ${MISSING[*]}"
fi

log "wsl-setup completado."
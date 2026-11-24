#!/usr/bin/env bash
# MettaOS SA-35 — smoke de compilación: make all, make iso (sin QEMU).
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

log() { printf '[smoke-build] %s\n' "$*" >&2; }

REQUIRED_TOOLS=(make gcc nasm ld)
OPTIONAL_ISO_TOOLS=(grub-mkrescue xorriso)

MISSING=()
for tool in "${REQUIRED_TOOLS[@]}"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    MISSING+=("$tool")
  fi
done

MAKE_ALL_EXIT=skipped
MAKE_ISO_EXIT=skipped
SMOKE_RESULT=UNKNOWN

if ((${#MISSING[@]} > 0)); then
  SMOKE_RESULT=INFRA_FAIL
  log "INFRA_FAIL: faltan en PATH: ${MISSING[*]}"
  log "Instale dependencias: chmod +x scripts/wsl-setup.sh && ./scripts/wsl-setup.sh"
  printf 'SMOKE_RESULT=%s\n' "$SMOKE_RESULT"
  printf 'MAKE_ALL_EXIT=%s\n' "$MAKE_ALL_EXIT"
  printf 'MAKE_ISO_EXIT=%s\n' "$MAKE_ISO_EXIT"
  exit 2
fi

for tool in "${OPTIONAL_ISO_TOOLS[@]}"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    log "ADVERTENCIA: $tool no encontrado; make iso puede fallar."
  fi
done

log "Directorio: $ROOT"
log "Ejecutando: make all"
set +e
make all
MAKE_ALL_EXIT=$?
set -e
log "make all → código de salida: $MAKE_ALL_EXIT"

log "Ejecutando: make iso"
set +e
make iso
MAKE_ISO_EXIT=$?
set +e
log "make iso → código de salida: $MAKE_ISO_EXIT"

printf 'SMOKE_RESULT='
if [[ "$MAKE_ALL_EXIT" -eq 0 && "$MAKE_ISO_EXIT" -eq 0 ]]; then
  SMOKE_RESULT=PASS
  printf '%s\n' "$SMOKE_RESULT"
  printf 'MAKE_ALL_EXIT=%s\n' "$MAKE_ALL_EXIT"
  printf 'MAKE_ISO_EXIT=%s\n' "$MAKE_ISO_EXIT"
  log "PASS smoke-build (binario + ISO generados)."
  exit 0
fi

SMOKE_RESULT=BUILD_FAIL
printf '%s\n' "$SMOKE_RESULT"
printf 'MAKE_ALL_EXIT=%s\n' "$MAKE_ALL_EXIT"
printf 'MAKE_ISO_EXIT=%s\n' "$MAKE_ISO_EXIT"
log "FAIL smoke-build (revisar salida de make arriba)."
exit 1
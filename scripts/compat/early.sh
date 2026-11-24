#!/bin/sh
# Debian live-boot mounts its read-only root and supplies the matching modules.
set -e
printf '\n%s\n' 'mkdir -p "${rootmnt}/opt/metta" && cp -a /metta/. "${rootmnt}/opt/metta/"' >> /scripts/init-bottom/ORDER
exec /init "$@"

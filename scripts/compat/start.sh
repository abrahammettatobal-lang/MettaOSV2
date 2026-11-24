#!/bin/bash
# PID 1 in the compatibility VM. Only the two virtual media and its own home
# volume are available here. MFS belongs to the native kernel.
set -eu
trap 'rc=$?; if ((rc)); then echo "Linux startup stopped ($rc)"; sync; poweroff -f; fi' EXIT
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
hostname metta-linux
mountpoint -q /dev/pts || mount -t devpts devpts /dev/pts
mkdir -p /dev/shm /tmp /run/metta /opt/AppImages /media/metta
mountpoint -q /dev/shm || mount -t tmpfs tmpfs /dev/shm
chmod 1777 /tmp /dev/shm
install -d -m 1777 /tmp/.X11-unix /tmp/.ICE-unix
modprobe virtio_console
modprobe virtio_blk
modprobe fuse || true
modprobe virtio_net || true
if [[ -c /dev/fuse ]]; then chmod 0666 /dev/fuse; fi
for node in /sys/class/net/*; do
    dev=${node##*/}
    ip link set "$dev" up
    if [[ $dev != lo ]]; then
        ip addr replace 10.0.2.15/24 dev "$dev"
        ip route replace default via 10.0.2.2 dev "$dev"
    fi
done
rm -f /etc/resolv.conf
printf 'nameserver 10.0.2.3\n' > /etc/resolv.conf
id metta >/dev/null 2>&1 || useradd --no-create-home -s /bin/bash metta
mkdir -p /home/metta
for ((i=0; i<100; i++)); do
    for port in /sys/class/virtio-ports/*; do
        [[ -f "$port/name" && $(cat "$port/name") == org.metta.mde ]] || continue
        export METTA_PORT="/dev/${port##*/}"
        if [[ ! -c $METTA_PORT ]]; then
            IFS=: read -r major minor < "$port/dev"
            mknod "$METTA_PORT" c "$major" "$minor"
        fi
    done
    [[ -e /dev/vda && -n ${METTA_PORT:-} ]] && break
    sleep .1
done
if [[ -z ${METTA_PORT:-} ]]; then
    ls -l /sys/class/virtio-ports /dev/vport* || true
    echo "Missing virtio port org.metta.mde" >&2
    exit 1
fi
if [[ $(cat /sys/class/block/vda/serial) != METTA_HOME ]]; then
    echo "Missing compatibility home volume" >&2
    exit 1
fi
# The launcher writes this exact marker only when it creates a new file.
if cmp -s -n 16 /dev/vda <(printf METTA-EMPTY-HOME); then
    mkfs.ext4 -q -F -L METTA_HOME /dev/vda
fi
mount -t ext4 /dev/vda /home/metta
chown metta:metta /home/metta /run/metta "$METTA_PORT"
chmod 700 /run/metta
if [[ -b /dev/sr1 ]]; then
    mount -t iso9660 -o ro,norock,mode=0555 /dev/sr1 /media/metta
    python3 /opt/metta/session.py --catalog
fi
export HOME=/home/metta USER=metta LOGNAME=metta SHELL=/bin/bash
export XDG_RUNTIME_DIR=/run/metta LANG=C.UTF-8 LIBGL_ALWAYS_SOFTWARE=1
cd "$HOME"
echo "MDE Linux session starting"
setpriv --reuid=metta --regid=metta --init-groups python3 -u /opt/metta/session.py
sync
poweroff -f

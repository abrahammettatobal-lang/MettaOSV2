#!/usr/bin/env bash
# Prueba headless: arranca la ISO, ejecuta comandos de monitor y guarda capturas.
# Uso: agent_smoke.sh <segundos_arranque> [archivo_de_comandos]
ROOT=/mnt/c/Users/USER/Desktop/MettaOS
OUT=$ROOT/build/smoke
mkdir -p "$OUT"
rm -f "$OUT"/*.ppm "$OUT"/serial.log /tmp/metta-mon.sock
cp -f "$ROOT/build/mettaos.iso" /tmp/mettaos-smoke.iso

ACCEL="-accel tcg"
[ -w /dev/kvm ] && ACCEL="-accel kvm"

qemu-system-x86_64 -m 1024 -no-reboot -display none \
  -drive file=/tmp/mettaos-smoke.iso,format=raw,if=ide \
  -boot order=c -machine pc,vmport=on -rtc base=localtime $ACCEL \
  -serial file:"$OUT/serial.log" \
  -monitor unix:/tmp/metta-mon.sock,server,nowait \
  -qmp unix:/tmp/metta-qmp.sock,server,nowait &
QPID=$!

sleep "${1:-12}"

mon() {
  python3 - "$1" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX)
s.connect("/tmp/metta-mon.sock")
s.settimeout(0.3)
try:
    s.recv(4096)
except Exception:
    pass
s.sendall((sys.argv[1] + "\n").encode())
time.sleep(0.15)
try:
    s.recv(4096)
except Exception:
    pass
s.close()
PY
}

qmp() {
  python3 - "$@" <<'PY'
import json, socket, sys, time
s = socket.socket(socket.AF_UNIX)
s.connect("/tmp/metta-qmp.sock")
f = s.makefile("rw")
f.readline()
def cmd(c, a=None):
    m = {"execute": c}
    if a is not None:
        m["arguments"] = a
    f.write(json.dumps(m) + "\n"); f.flush()
    while True:
        r = json.loads(f.readline())
        if "return" in r or "error" in r:
            return r
cmd("qmp_capabilities")
op = sys.argv[1]
def btn(b, down):
    cmd("input-send-event", {"events": [{"type": "btn", "data": {"down": down, "button": b}}]})
if op == "abs":
    x, y = int(sys.argv[2]), int(sys.argv[3])
    cmd("input-send-event", {"events": [
        {"type": "abs", "data": {"axis": "x", "value": x * 32767 // 1023}},
        {"type": "abs", "data": {"axis": "y", "value": y * 32767 // 767}}]})
elif op in ("click", "dclick", "rclick"):
    b = "right" if op == "rclick" else "left"
    for _ in range(2 if op == "dclick" else 1):
        btn(b, True); time.sleep(0.12); btn(b, False); time.sleep(0.12)
elif op == "down":
    btn("left", True)
elif op == "up":
    btn("left", False)
elif op == "wheel":
    b = "wheel-down" if int(sys.argv[2]) > 0 else "wheel-up"
    for _ in range(abs(int(sys.argv[2]))):
        btn(b, True); btn(b, False); time.sleep(0.05)
s.close()
PY
}

mon "screendump $OUT/shot0.ppm"
n=1
if [ -n "$2" ] && [ -f "$2" ]; then
  while IFS= read -r line; do
    case "$line" in
      ""|\#*) continue ;;
      sleep\ *) sleep "${line#sleep }" ;;
      shot) mon "screendump $OUT/shot$n.ppm"; n=$((n+1)) ;;
      abs\ *|click|dclick|rclick|down|up|wheel\ *) qmp $line; sleep 0.2 ;;
      type\ *)
        t="${line#type }"
        for ((i=0; i<${#t}; i++)); do
          ch="${t:i:1}"
          case "$ch" in
            " ") k=spc ;; ".") k=dot ;; "-") k=slash ;; "/") k=shift-7 ;;
            "*") k=shift-bracket_right ;; "+") k=bracket_right ;; "=") k=shift-0 ;;
            [A-Z]) k="shift-${ch,,}" ;;
            *) k="$ch" ;;
          esac
          mon "sendkey $k"
        done ;;
      *) mon "$line"; sleep 0.15 ;;
    esac
  done < "$2"
fi
sleep 0.5
kill $QPID 2>/dev/null
wait $QPID 2>/dev/null
echo "accel=$ACCEL"
ls "$OUT"
tail -n 15 "$OUT/serial.log"

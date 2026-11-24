"""Boot twice with a disposable MFS disk and check the MDE save and RTL8139 paths."""
import argparse
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MARKER = "mettapersist"


def boot(qemu, iso, disk, directory, number, memory):
    serial = directory / f"boot-{number}.log"
    args = [qemu, "-m", str(memory), "-accel", "tcg", "-display", "none",
            "-monitor", "stdio", "-serial", f"file:{serial}",
            "-drive", f"file={str(iso).replace(',', ',,')},format=raw,if=ide,index=0,snapshot=on",
            "-drive", f"file={str(disk).replace(',', ',,')},format=raw,if=ide,index=1",
            "-nic", "user,model=rtl8139", "-no-reboot"]
    with subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                          text=True) as vm:
        try:
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                log = serial.read_text(errors="replace") if serial.exists() else ""
                if "gateway ARP confirmed" in log:
                    break
                if vm.poll() is not None or "CPU exception" in log:
                    raise RuntimeError(log[-1200:] or "QEMU exited during boot")
                time.sleep(0.25)
            else:
                raise TimeoutError(log[-1200:])
            assert "root MFS on persistent ATA disk mounted" in log
            if number == 1:
                time.sleep(1)
                for key in ("ctrl-spc", "n", "o", "t", "a", "s", "ret", "ctrl-n"):
                    vm.stdin.write(f"sendkey {key} 60\n")
                    vm.stdin.flush()
                    time.sleep(.28)
                for letter in MARKER:
                    vm.stdin.write(f"sendkey {letter} 50\n")
                    vm.stdin.flush()
                    time.sleep(0.15)
                time.sleep(1)
            else:
                assert "restored desktop from MFS" in log
            vm.stdin.write("quit\n")
            vm.stdin.flush()
            vm.wait(timeout=10)
        finally:
            if vm.poll() is None:
                vm.kill()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", type=Path, default=ROOT / "build/mettaos.iso")
    parser.add_argument("--qemu", default=shutil.which("qemu-system-x86_64") or
                        r"C:\Program Files\qemu\qemu-system-x86_64.exe")
    parser.add_argument("--memory", type=int, default=6144)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        directory = Path(tmp)
        disk = directory / "userdata.img"
        subprocess.run([sys.executable, str(ROOT / "scripts/create-data-disk.py"), str(disk)],
                       check=True)
        boot(args.qemu, args.iso.resolve(), disk, directory, 1, args.memory)
        assert MARKER.encode() in disk.read_bytes(), "Note missing from the disk"
        boot(args.qemu, args.iso.resolve(), disk, directory, 2, args.memory)
    print("PASS: RTL8139 gateway, MDE text write, second-boot MFS recovery")


if __name__ == "__main__":
    main()

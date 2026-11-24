"""Arranca el escritorio de MettaOS. Otra máquina Linux solo si se pasa --linux-iso."""
import argparse
import contextlib
import gzip
import hashlib
import json
import os
import runpy
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
COMPAT = ROOT / "scripts/compat"
CACHE = ROOT / "build/compat"


def host_path(value):
    """Acepta rutas de Windows aunque este script corra dentro de WSL."""
    text = str(value)
    if os.name != "nt" and len(text) >= 2 and text[0].isalpha() and text[1] == ":":
        rest = text[2:].replace("\\", "/").lstrip("/")
        return Path(f"/mnt/{text[0].lower()}/{rest}")
    return Path(value)


def find_tool(name):
    found = shutil.which(name)
    if not found and os.name == "nt":
        for folder in ("qemu", "7-Zip"):
            candidate = Path(os.environ["ProgramFiles"]) / folder / (name + ".exe")
            if candidate.is_file():
                return str(candidate)
    return found


def fingerprint(paths):
    return hashlib.sha256(json.dumps([(str(p.resolve()), p.stat().st_size,
                                       p.stat().st_mtime_ns) for p in paths]).encode()).hexdigest()[:20]


def extract_boot(iso, name, destination):
    # Both 7-Zip and bsdtar can extract a single ISO file without mounting it.
    seven = find_tool("7z")
    command = [seven, "x", "-so", str(iso), "live/" + name] if seven else [
        "bsdtar", "-xOf", str(iso), "live/" + name]
    if not seven and not shutil.which("bsdtar"):
        subprocess.run(["xorriso", "-osirrox", "on", "-indev", str(iso), "-extract",
                        "/live/" + name, str(destination)], check=True)
    else:
        with destination.open("wb") as output:
            subprocess.run(command, stdout=output, check=True)
    if destination.stat().st_size < 1024:
        raise ValueError(f"La ISO debe contener live/{name} (Debian Live x86_64).")


def newc(entries):
    result = bytearray()
    for ino, (name, mode, data) in enumerate([*entries, ("TRAILER!!!", 0, b"")], 1):
        name = name.encode() + b"\0"
        fields = (ino, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(name), 0)
        result += b"070701" + b"".join(f"{x:08x}".encode() for x in fields) + name
        result += bytes(-len(result) % 4) + data
        result += bytes(-len(result) % 4)
    return result + bytes(-len(result) % 512)


def prepare_runtime(iso):
    sources = [COMPAT / p for p in ("early.sh", "start.sh", "session.py")]
    target = CACHE / ("boot-" + fingerprint([iso, *sources]))
    if not target.is_dir():
        with tempfile.TemporaryDirectory(dir=CACHE) as tmp:
            stage = Path(tmp) / "boot"
            stage.mkdir()
            extract_boot(iso, "vmlinuz", stage / "kernel")
            extract_boot(iso, "initrd.img", stage / "initrd")
            entries = [("metta", 0o40755, b"")]
            for path, name in zip(sources, ("metta-early", "metta/start", "metta/session.py")):
                entries.append((name, 0o100755, path.read_bytes().replace(b"\r\n", b"\n")))
            with (stage / "initrd").open("ab") as output:
                output.write(bytes(-output.tell() % 4))
                output.write(gzip.compress(newc(entries), mtime=0))
            try:
                stage.rename(target)
            except FileExistsError:
                pass
    return target


def app_media(paths):
    if not paths:
        return None
    names = set()
    if len(paths) > 64:
        raise ValueError("MDE admite hasta 64 AppImages por sesión.")
    for p in paths:
        with p.open("rb") as source:
            header = source.read(20)
        if (len(header) < 20 or header[:6] != b"\x7fELF\x02\x01" or
                header[8:11] not in (b"AI\x01", b"AI\x02") or header[18:20] != b"\x3e\x00"):
            raise ValueError(f"{p.name}: se requiere un AppImage x86_64 tipo 1 o 2.")
        if p.name in names or len(p.name.encode()) > 255 or any(ord(c) < 32 for c in p.name):
            raise ValueError(f"Nombre de AppImage duplicado o inválido: {p.name!r}")
        names.add(p.name)
    target = CACHE / ("apps-" + fingerprint(paths) + ".iso")
    if target.exists():
        return target
    with tempfile.TemporaryDirectory(dir=CACHE) as tmp:
        stage = Path(tmp) / "files"
        stage.mkdir()
        for i, p in enumerate(paths):
            dest = stage / f"a{i:04d}.bin"
            try:
                os.link(p, dest)
            except OSError:
                shutil.copyfile(p, dest)
        (stage / "index.jsn").write_text(json.dumps([p.name for p in paths]), encoding="ascii")
        result = Path(tmp) / "apps.iso"
        if shutil.which("xorriso"):
            cmd = ["xorriso", "-as", "mkisofs", "-quiet", "-V", "METTA_APPS", "-o", result, stage]
        else:
            cmd = [find_tool("bsdtar") or "tar", "--format", "iso9660", "-cf", result, "-C", stage, "."]
        subprocess.run(cmd, check=True)
        os.replace(result, target)
    return target


def fileopt(path):
    return str(path.resolve()).replace(",", ",,")


def relay(source, dest):
    try:
        while block := os.read(source.fileno(), 65536):
            view = memoryview(block)
            while view:
                view = view[os.write(dest.fileno(), view):]
    except (OSError, ValueError):
        pass


def stop(vm):
    if vm.poll() is None:
        vm.terminate()
        try:
            vm.wait(timeout=5)
        except subprocess.TimeoutExpired:
            vm.kill()
            vm.wait()


def linux_args(qemu, iso, boot, apps, home_disk, log):
    return [qemu, "-name", "MettaOS Linux", "-machine", "pc", "-m", "2048", "-smp", "2",
            "-cpu", "max", "-display", "none", "-monitor", "none", "-no-reboot",
            "-serial", "file:" + str(log), "-kernel", str(boot / "kernel"), "-initrd", str(boot / "initrd"),
            "-append", "boot=live components nopersistence noeject rdinit=/metta-early "
                       "init=/opt/metta/start console=ttyS0 noapic quiet",
            "-drive", f"file={fileopt(iso)},format=raw,media=cdrom,if=ide,index=2,readonly=on",
            "-drive", f"file={fileopt(home_disk)},format=raw,if=none,id=home",
            "-device", "virtio-blk-pci,drive=home,serial=METTA_HOME",
            "-nic", "user,model=virtio-net-pci", "-chardev", "stdio,id=link,signal=off",
            "-device", "virtio-serial-pci", "-device", "virtserialport,chardev=link,name=org.metta.mde",
            *(["-drive", f"file={fileopt(apps)},format=raw,media=cdrom,if=ide,index=3,readonly=on"] if apps else [])]


def make_home(path):
    if path.exists():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=path.parent) as tmp:
        new = Path(tmp) / "home.img"
        with new.open("wb") as output:
            output.write(b"METTA-EMPTY-HOME")
            output.truncate(1024 * 1024 * 1024)
        try:
            os.link(new, path)
        except FileExistsError:
            pass


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--iso", type=Path, default=ROOT / "build/mettaos.iso")
    p.add_argument("--disk", type=Path, default=ROOT / "data/mettaos.img")
    p.add_argument("--linux-iso", type=Path)
    p.add_argument("--appimage", type=Path, action="append", default=[])
    p.add_argument("--native-only", action="store_true")
    p.add_argument("--compat", action="store_true", help="Use the saved Linux desktop profile")
    p.add_argument("--prepare", action="store_true", help="Record a local Debian Live ISO for future launches")
    p.add_argument("--qemu", default=find_tool("qemu-system-x86_64"))
    p.add_argument("--memory", type=int, default=6144)
    args = p.parse_args()
    CACHE.mkdir(parents=True, exist_ok=True)
    config = ROOT / "data/linux-runtime.json"
    live = host_path(args.linux_iso) if args.linux_iso else None
    configured_images = []
    if args.compat and not live:
        if not config.is_file():
            p.error("Configura primero el perfil con --prepare --linux-iso.")
        profile = json.loads(config.read_text(encoding="utf-8"))
        live = host_path(profile["iso"])
        configured_images = [host_path(item) for item in profile.get("images", [])]
    if args.native_only and (args.appimage or args.linux_iso or args.compat):
        p.error("--native-only no admite medios Linux")
    images = [host_path(item) for item in (
        args.appimage or [*configured_images, *sorted((ROOT / "data/AppImages").glob("*.[Aa][Pp][Pp][Ii][Mm][Aa][Gg][Ee]"))])]
    if (args.appimage or args.prepare) and not live:
        p.error("Indica una ISO Debian Live con --linux-iso.")
    if live:
        live = host_path(live)
        if not live.is_file():
            p.error(f"No está la ISO Linux: {live}")
        live = live.resolve()
        boot = prepare_runtime(live)
        media = app_media(images)
        if args.prepare:
            config.parent.mkdir(parents=True, exist_ok=True)
            config.write_text(json.dumps({"iso": str(live), "images": [str(p.resolve()) for p in args.appimage]}, indent=2), encoding="utf-8")
            return 0
    if not args.qemu:
        p.error("Instala QEMU x86_64 o pasa --qemu.")
    if not args.disk.exists():
        subprocess.run([sys.executable, str(ROOT / "scripts/create-data-disk.py"), str(args.disk)], check=True)
    main_vm = [args.qemu, "-name", "MettaOS", "-machine", "pc", "-rtc", "base=localtime",
               "-m", str(args.memory), "-nic", "user,model=rtl8139", "-boot", "order=c",
               "-drive", f"file={fileopt(args.iso)},format=raw,if=ide,index=0,snapshot=on",
               "-drive", f"file={fileopt(args.disk)},format=raw,if=ide,index=1"]
    if not live:
        if os.name == "nt":
            return subprocess.call([sys.executable, str(ROOT / "scripts/qemu-windows-dns.py"),
                                    *main_vm, "-serial", "stdio"])
        return subprocess.call(main_vm + ["-serial", "stdio"])
    home_disk = ROOT / "data/linux-home.img"
    make_home(home_disk)
    main_vm += ["-monitor", "none", "-serial", "file:" + str(CACHE / "mde.log"),
                "-chardev", "stdio,id=link,signal=off", "-device",
                "virtio-serial-pci,disable-modern=on,vectors=0", "-device", "virtconsole,chardev=link"]
    if os.name == "nt":
        dns_pipe = "metta-dns-" + uuid.uuid4().hex
        main_vm += ["-serial", "pipe:" + dns_pipe]
        connect_dns = runpy.run_path(str(ROOT / "scripts/qemu-windows-dns.py"))["connect"]
    with contextlib.ExitStack() as stack:
        guest = subprocess.Popen(linux_args(args.qemu, live, boot, media, home_disk, CACHE / "linux.log"),
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)
        stack.callback(stop, guest)
        desktop = subprocess.Popen(main_vm, stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)
        stack.callback(stop, desktop)
        if os.name == "nt":
            threading.Thread(target=connect_dns, args=(desktop, dns_pipe), daemon=True).start()
        for source, dest in ((desktop.stdout, guest.stdin), (guest.stdout, desktop.stdin)):
            threading.Thread(target=relay, args=(source, dest), daemon=True).start()
        warned = False
        while desktop.poll() is None:
            if guest.poll() is not None and not warned:
                print(f"Linux terminó. Consulta {CACHE / 'linux.log'}.", file=sys.stderr)
                warned = True
            time.sleep(.2)
        # A private shutdown frame lets PID 1 sync and unmount its home volume.
        if guest.poll() is None:
            try:
                guest.stdin.write(b"Q\x00\x00\x00")
                guest.wait(timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                pass
        return desktop.returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        sys.exit(str(exc))

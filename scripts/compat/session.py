"""X11 sessions inside the Linux VM. The virtio port carries input and dirty strips."""
import ctypes as C
import json
import mmap
import os
from pathlib import Path
import select
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import time
import zlib

WIDTH, HEIGHT, STRIP = 800, 480, 8
INDEX = Path("/media/metta/index.jsn")
APPS = Path("/opt/AppImages")
HEADER = struct.Struct("<BBH")


def catalog():
    return json.loads(INDEX.read_text()) if INDEX.is_file() else []


def link_images():
    for i, name in enumerate(catalog()):
        if name != Path(name).name or name in ("", ".", ".."):
            raise ValueError("Invalid AppImage name")
        (APPS / name).symlink_to(INDEX.parent / f"a{i:04d}.bin")


def send(kind, slot=0, data=b""):
    frame = memoryview(HEADER.pack(ord(kind), slot, len(data)) + data)
    while frame:
        frame = frame[os.write(port, frame):]


def status(slot, message):
    print(f"[{slot}] {message}", flush=True)
    send("S", slot, message.encode()[:240])


def bind(lib, name, result, *params):
    func = getattr(lib, name)
    func.argtypes, func.restype = params, result
    return func


class Display:
    def __init__(self, number):
        self.env = dict(os.environ, DISPLAY=f":{number}", GSK_RENDERER="cairo")
        self.dir = tempfile.TemporaryDirectory(dir=os.environ["XDG_RUNTIME_DIR"])
        self.server = subprocess.Popen(["Xvfb", f":{number}", "-screen", "0", f"{WIDTH}x{HEIGHT}x24",
                                        "-fbdir", self.dir.name, "-nolisten", "tcp", "-noreset"],
                                       start_new_session=True)
        self.display = None
        self.fb = None
        self.child = None
        self.previous = [None] * (HEIGHT // STRIP)
        self.keys, self.buttons = set(), set()
        try:
            for _ in range(160):
                if self.server.poll() is not None:
                    raise RuntimeError("Xvfb terminó al iniciar")
                sock = Path(f"/tmp/.X11-unix/X{number}")
                if sock.exists():
                    self.display = xopen(f":{number}".encode())
                    if self.display:
                        break
                time.sleep(.05)
            else:
                raise RuntimeError("X11 no respondió")
            with (Path(self.dir.name) / "Xvfb_screen0").open("rb") as f:
                self.fb = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
            fields = struct.unpack_from(">25I", self.fb)
            self.offset = fields[0] + 12 * fields[19]
            self.stride = fields[12]
            if ((fields[1], fields[4], fields[5], fields[7], fields[11], self.stride) !=
                    (7, WIDTH, HEIGHT, 0, 32, WIDTH * 4) or self.offset + HEIGHT * self.stride > len(self.fb)):
                raise RuntimeError("Formato de pantalla X11 incompatible")
            self.root = xroot(self.display)
            self.windows = set()
        except BaseException:
            self.close()
            raise

    def focus_windows(self):
        root, parent, children, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
        if not xquery(self.display, self.root, C.byref(root), C.byref(parent),
                      C.byref(children), C.byref(count)):
            return
        # Only mapped, managed top-level windows can take keyboard focus.
        mapped = []
        try:
            for i in range(count.value):
                attr = WindowAttributes()
                win = children[i]
                if xattrs(self.display, win, C.byref(attr)) and attr.map_state == 2 and not attr.override_redirect:
                    mapped.append(win)
        finally:
            if children:
                xfree(children)
        current = set(mapped)
        if mapped and (current - self.windows or self.windows - current):
            last = mapped[-1]
            if not self.windows and len(mapped) == 1:
                xresize(self.display, last, 0, 0, WIDTH, HEIGHT)
            xfocus(self.display, last, 1, 0)
            xflush(self.display)
            print(f"X11 mapped {len(mapped)} window(s) on {self.env['DISPLAY']}", flush=True)
        self.windows = current

    def frame(self, slot, reset):
        self.focus_windows()
        for i in range(len(self.previous)):
            pos = self.offset + i * STRIP * self.stride
            pixels = self.fb[pos:pos + STRIP * self.stride]
            if reset or pixels != self.previous[i]:
                self.previous[i] = pixels
                send("G", slot, struct.pack("<H", i * STRIP) + zlib.compress(pixels, 1))
        send("E", slot)

    def input(self, kind, data):
        if kind == "M" and len(data) == 4:
            x, y = struct.unpack("<HH", data)
            xmotion(self.display, 0, min(x, WIDTH - 1), min(y, HEIGHT - 1), 0)
        elif kind in ("K", "P") and len(data) == 2:
            code, down = data
            if kind == "K" and 8 <= code <= 255:
                xkey(self.display, code, bool(down), 0)
                held = self.keys
            elif kind == "P" and 1 <= code <= 5:
                xbutton(self.display, code, bool(down), 0)
                held = self.buttons
            else:
                return
            (held.add if down else held.discard)(code)
        elif kind == "U":
            for code in self.keys:
                xkey(self.display, code, False, 0)
            for code in self.buttons:
                xbutton(self.display, code, False, 0)
            self.keys.clear()
            self.buttons.clear()
        xflush(self.display)

    def close(self):
        if self.display:
            xclose(self.display)
            self.display = None
        for proc in (self.child, self.server):
            if proc is not None:
                try:
                    os.killpg(proc.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
        if self.fb is not None:
            self.fb.close()
        for proc in (self.child, self.server):
            if proc is not None:
                try:
                    proc.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    os.killpg(proc.pid, signal.SIGKILL)
                    proc.wait()
        self.dir.cleanup()


class WindowAttributes(C.Structure):
    _fields_ = [(n, C.c_int) for n in ("x", "y", "w", "h", "border", "depth")] + [
        ("visual", C.c_void_p), ("root", C.c_ulong)] + [(n, C.c_int) for n in
        ("cls", "bit_gravity", "win_gravity", "backing_store")] + [
        ("planes", C.c_ulong), ("pixel", C.c_ulong), ("save_under", C.c_int),
        ("colormap", C.c_ulong), ("installed", C.c_int), ("map_state", C.c_int),
        ("all_events", C.c_long), ("your_events", C.c_long), ("do_not_propagate", C.c_long),
        ("override_redirect", C.c_int), ("screen", C.c_void_p)]


def shell_command():
    return ["xterm", "-u8", "-fa", "DejaVu Sans Mono", "-fs", "11", "-bg", "#162733",
            "-fg", "#eef5f7", "-cr", "#79d8cb", "-bc", "-sb", "-sl", "4000",
            "-xrm", "XTerm*selectToClipboard: true", "-e", "/bin/bash", "-i"]


def run_image(name, env):
    path = APPS / name
    with path.open("rb") as f:
        header = f.read(20)
    if (header[:6] != b"\x7fELF\x02\x01" or header[18:20] != b"\x3e\x00"
            or header[8:11] not in (b"AI\x01", b"AI\x02")):
        raise ValueError("Se requiere un AppImage x86_64 tipo 1 o 2")
    # Type 1 images contain ISO9660. Direct extraction also supports old
    # runtimes that depend on a system copy of libarchive or FUSE 2.
    if header[10] == 1:
        directory = tempfile.mkdtemp(prefix="type1-", dir=env["XDG_RUNTIME_DIR"])
        if shutil.which("bsdtar"):
            command = ["bsdtar", "-xf", str(path), "-C", directory]
        else:
            command = ["xorriso", "-osirrox", "on", "-indev", str(path), "-extract", "/", directory]
        try:
            subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
        except BaseException:
            shutil.rmtree(directory)
            raise
        env.update(APPIMAGE=str(path), APPDIR=directory, OWD=os.getcwd(), ARGV0=str(path))
        return [str(Path(directory) / "AppRun")], directory
    if not os.access("/dev/fuse", os.R_OK | os.W_OK):
        env["APPIMAGE_EXTRACT_AND_RUN"] = "1"
    return [str(path)], None


def close_session(slot):
    if slot in sessions:
        display, extracted = sessions.pop(slot)
        display.close()
        if extracted:
            shutil.rmtree(extracted, ignore_errors=True)
    if slot in uploads:
        upload = uploads.pop(slot)
        upload.close()
        Path(upload.name).unlink(missing_ok=True)


def dispatch(kind, slot, data):
    if kind == "?" and not data:
        send("H", 0, b"MDE1")
        for i, name in enumerate(names):
            send("I", i, name.encode())
        send("L")
    elif kind == "Q" and slot == 0 and not data:
        return False
    elif 1 <= slot <= 12:
        if kind == "V":
            close_session(slot)
            uploads[slot] = tempfile.NamedTemporaryFile(dir=os.environ["HOME"], prefix=".metta-", suffix=".AppImage", delete=False)
        elif kind == "D" and slot in uploads:
            uploads[slot].write(data)
        elif kind in ("B", "A", "Z"):
            if kind == "A" and (len(data) != 1 or data[0] >= len(names)):
                raise ValueError("El AppImage ya no está en el medio")
            if kind == "Z":
                if slot not in uploads: return True
                uploads[slot].close()
                Path(uploads[slot].name).chmod(0o500)
            else:
                close_session(slot)
            screen = Display(slot)
            sessions[slot] = (screen, None)
            if kind in ("A", "Z"):
                path = names[data[0]] if kind == "A" else uploads[slot].name
                cmd, extracted = run_image(path, screen.env)
                sessions[slot] = (screen, extracted)
            else:
                cmd = shell_command()
            screen.child = subprocess.Popen(["dbus-run-session", "--", *cmd], env=screen.env, start_new_session=True)
            print(f"Starting {cmd[0]} on :{slot}", flush=True)
            send("O", slot)
        elif kind == "C":
            close_session(slot)
        elif slot in sessions:
            screen = sessions[slot][0]
            if kind == "F":
                screen.frame(slot, bool(data and data[0]))
            else:
                screen.input(kind, data)
    return True


def serve():
    incoming = bytearray()
    while True:
        if select.select([port], [], [], .25)[0]:
            block = os.read(port, 8192)
            if not block:
                return
            incoming.extend(block)
        while len(incoming) >= 4:
            kind, slot, length = HEADER.unpack_from(incoming)
            if len(incoming) < length + 4:
                break
            data = bytes(incoming[4:4 + length])
            del incoming[:4 + length]
            try:
                if not dispatch(chr(kind), slot, data):
                    return
            except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
                close_session(slot)
                status(slot, str(exc))
        for slot, (screen, _) in list(sessions.items()):
            code = screen.child.poll()
            if code is not None:
                close_session(slot)
                if code:
                    status(slot, f"El proceso terminó con código {code}.")
                else:
                    send("X", slot)


if __name__ == "__main__":
    if sys.argv[1:] == ["--catalog"]:
        link_images()
        sys.exit(0)
    xl = C.CDLL("libX11.so.6")
    xt = C.CDLL("libXtst.so.6")
    ptr, ul, ui, integer = C.c_void_p, C.c_ulong, C.c_uint, C.c_int
    xopen = bind(xl, "XOpenDisplay", ptr, C.c_char_p)
    xclose = bind(xl, "XCloseDisplay", integer, ptr)
    xroot = bind(xl, "XDefaultRootWindow", ul, ptr)
    xquery = bind(xl, "XQueryTree", integer, ptr, ul, ptr, ptr, ptr, ptr)
    xattrs = bind(xl, "XGetWindowAttributes", integer, ptr, ul, ptr)
    xresize = bind(xl, "XMoveResizeWindow", integer, ptr, ul, integer, integer, ui, ui)
    xfocus = bind(xl, "XSetInputFocus", integer, ptr, ul, integer, ul)
    xfree = bind(xl, "XFree", integer, ptr)
    xflush = bind(xl, "XFlush", integer, ptr)
    xkey = bind(xt, "XTestFakeKeyEvent", integer, ptr, ui, integer, ul)
    xbutton = bind(xt, "XTestFakeButtonEvent", integer, ptr, ui, integer, ul)
    xmotion = bind(xt, "XTestFakeMotionEvent", integer, ptr, integer, integer, integer, ul)
    # A window can disappear between XQueryTree and XGetWindowAttributes.
    error_handler = C.CFUNCTYPE(integer, ptr, ptr)(lambda display, event: 0)
    bind(xl, "XSetErrorHandler", ptr, ptr)(error_handler)
    port = os.open(os.environ["METTA_PORT"], os.O_RDWR)
    names, sessions, uploads = catalog(), {}, {}
    try:
        serve()
    finally:
        for number in set(sessions) | set(uploads):
            close_session(number)
        os.close(port)

"""Attach the Windows IPv4 resolver to the guest's second serial port."""
import re
import socket
import subprocess
import sys
import threading
import time
import uuid


def serve(pipe):
    while line := pipe.readline(300):
        match = re.fullmatch(rb"D([0-9a-f]{4}) ([A-Za-z0-9.-]{1,253})\n", line)
        if not match:
            continue
        ticket, name = match.groups()
        try:
            records = socket.getaddrinfo(name.decode("ascii"), None, socket.AF_INET,
                                         socket.SOCK_DGRAM)
            address = records[0][4][0].encode("ascii")
        except (OSError, UnicodeError, IndexError):
            address = b"!"
        pipe.write(b"D" + ticket + b" " + address + b"\n")
        pipe.flush()


def connect(vm, name):
    while vm.poll() is None:
        try:
            pipe = open(r"\\.\pipe" + "\\" + name, "r+b", buffering=0)
        except OSError:
            time.sleep(0.1)
            continue
        try:
            with pipe:
                serve(pipe)
        except OSError:
            pass
        return


def main():
    name = "metta-dns-" + uuid.uuid4().hex
    with subprocess.Popen([*sys.argv[1:], "-serial", "pipe:" + name]) as vm:
        threading.Thread(target=connect, args=(vm, name), daemon=True).start()
        try:
            return vm.wait()
        except KeyboardInterrupt:
            vm.terminate()
            return vm.wait()


if __name__ == "__main__":
    sys.exit(main())

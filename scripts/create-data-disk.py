"""Create the QEMU primary-slave MFS image.

Without a Firefox AppImage the volume stays 32 MiB, enough for the desktop
catalogue. When the AppImage is present the volume grows to the bitmap limit
(128 MiB of data blocks) and stores the file at /home/usuario/firefox and
/home/usuario/Documentos/Firefox.AppImage, same inode.
"""
import os
from pathlib import Path
import struct
import sys
import tempfile
import zlib

BLOCK = 4096
SMALL = 32 * 1024 * 1024
VOLUME_START = BLOCK
INODE_TABLE = 3
DATA_START = 11
DATA_CAP = 32768  # one bitmap block
PTRS = BLOCK // 4
APP_NAME = "firefox-beta-161.0.r20261118090429-x86_64.AppImage"


def checksum(record, offset):
    crc = zlib.crc32(record[:offset] + record[offset + 4:]) & 0xFFFFFFFF
    struct.pack_into("<I", record, offset, crc)


def find_appimage():
    env = os.environ.get("METTA_APPIMAGE")
    candidates = []
    if env:
        candidates.append(Path(env))
    candidates.append(Path(r"C:\Users\USER\Downloads") / APP_NAME)
    candidates.append(Path("/mnt/c/Users/USER/Downloads") / APP_NAME)
    for path in candidates:
        if path.is_file():
            return path
    return None


def inode(ino, mode, nlink, size, blocks, indirect, double_indirect):
    raw = bytearray(128)
    struct.pack_into("<IHHIII", raw, 0, ino, mode, nlink, 0, 0, size)
    for i, lba in enumerate(blocks[:12]):
        struct.pack_into("<I", raw, 20 + 4 * i, lba)
    struct.pack_into("<IIII", raw, 68, indirect, 1, 1, 1)
    struct.pack_into("<I", raw, 88, double_indirect)
    checksum(raw, 84)
    return raw


def dirent(buf, pos, ino, name, kind):
    raw = name.encode("ascii")
    rec = (8 + len(raw) + 3) & ~3
    struct.pack_into("<IHBB", buf, pos, ino, rec, len(raw), kind)
    buf[pos + 8:pos + 8 + len(raw)] = raw
    return pos + rec


def directory(entries):
    buf = bytearray(BLOCK)
    pos = 0
    for ino, name, kind in entries:
        pos = dirent(buf, pos, ino, name, kind)
    return buf, pos


class Volume:
    def __init__(self, disk, total):
        self.disk = disk
        self.total = total
        self.next = DATA_START
        self.used = []

    def alloc(self):
        if self.next >= self.total:
            raise RuntimeError("el bitmap de MFS no cabe el archivo")
        lba = self.next
        self.next += 1
        self.used.append(lba)
        return lba

    def write(self, lba, data):
        if len(data) != BLOCK:
            raise RuntimeError("bloque incompleto")
        self.disk.seek(VOLUME_START + lba * BLOCK)
        self.disk.write(data)


def write_pointers(vol, lbas):
    if not lbas:
        return 0
    block = bytearray(BLOCK)
    for i, lba in enumerate(lbas):
        struct.pack_into("<I", block, i * 4, lba)
    slot = vol.alloc()
    vol.write(slot, block)
    return slot


def embed_appimage(vol, disk, app, file_ino):
    size = app.stat().st_size
    if size <= 0 or size > 0xFFFFFFFF:
        raise RuntimeError("tamaño de AppImage fuera de MFS")
    blocks = (size + BLOCK - 1) // BLOCK
    data = []
    with app.open("rb") as src:
        for _ in range(blocks):
            chunk = src.read(BLOCK)
            if len(chunk) < BLOCK:
                chunk += bytes(BLOCK - len(chunk))
            lba = vol.alloc()
            disk.seek(VOLUME_START + lba * BLOCK)
            disk.write(chunk)
            data.append(lba)
    direct = data[:12]
    rest = data[12:]
    single = rest[:PTRS]
    rest = rest[PTRS:]
    indirect = write_pointers(vol, single)
    leaves = [write_pointers(vol, rest[i:i + PTRS]) for i in range(0, len(rest), PTRS)]
    double = write_pointers(vol, leaves)
    disk.seek(VOLUME_START + INODE_TABLE * BLOCK + file_ino * 128)
    disk.write(inode(file_ino, 0x81ED, 2, size, direct, indirect, double))
    return size


def format_image(disk, app):
    if app is None:
        total = (SMALL - VOLUME_START) // BLOCK
        data_cap = total - DATA_START
    else:
        data_cap = DATA_CAP
        total = DATA_START + data_cap
    disk.truncate(VOLUME_START + total * BLOCK)
    disk.write(b"MettaOS data volume v1\0")

    vol = Volume(disk, total)
    root_lba = vol.alloc()
    if app is not None:
        home_lba = vol.alloc()
        user_lba = vol.alloc()
        docs_lba = vol.alloc()
        root_body, root_size = directory(((1, ".", 2), (1, "..", 2), (2, "home", 2)))
        home_body, home_size = directory(((2, ".", 2), (1, "..", 2), (3, "usuario", 2)))
        user_body, user_size = directory(
            ((3, ".", 2), (2, "..", 2), (4, "Documentos", 2), (5, "firefox", 1)))
        docs_body, docs_size = directory(
            ((4, ".", 2), (3, "..", 2), (5, "Firefox.AppImage", 1)))
        vol.write(root_lba, root_body)
        vol.write(home_lba, home_body)
        vol.write(user_lba, user_body)
        vol.write(docs_lba, docs_body)
        embed_appimage(vol, disk, app, 5)
    else:
        root_body, root_size = directory(((1, ".", 2), (1, "..", 2)))
        vol.write(root_lba, root_body)
        home_lba = user_lba = docs_lba = 0
        home_size = user_size = docs_size = 0

    inodes = bytearray(8 * BLOCK)
    inodes[128:256] = inode(1, 0x4000, 2, root_size, [root_lba], 0, 0)
    if app is not None:
        inodes[2 * 128:3 * 128] = inode(2, 0x4000, 2, home_size, [home_lba], 0, 0)
        inodes[3 * 128:4 * 128] = inode(3, 0x4000, 2, user_size, [user_lba], 0, 0)
        inodes[4 * 128:5 * 128] = inode(4, 0x4000, 2, docs_size, [docs_lba], 0, 0)
        # inode 5 was written by embed_appimage; keep those bytes
        disk.seek(VOLUME_START + INODE_TABLE * BLOCK + 5 * 128)
        inodes[5 * 128:6 * 128] = disk.read(128)

    disk.seek(VOLUME_START + INODE_TABLE * BLOCK)
    disk.write(inodes)

    ibmap = bytearray(BLOCK)
    used_inodes = 6 if app is not None else 2
    for bit in range(used_inodes):
        ibmap[bit >> 3] |= 1 << (bit & 7)
    disk.seek(VOLUME_START + BLOCK)
    disk.write(ibmap)

    bmap = bytearray(BLOCK)
    for lba in vol.used:
        rel = lba - DATA_START
        bmap[rel >> 3] |= 1 << (rel & 7)
    disk.seek(VOLUME_START + 2 * BLOCK)
    disk.write(bmap)

    free_blocks = data_cap - len(vol.used)
    superblock = bytearray(BLOCK)
    struct.pack_into("<13I", superblock, 0, 0x4D465331, 1, BLOCK, total, 256,
                     INODE_TABLE, 8, DATA_START, 1, 2, 1, 256 - used_inodes, free_blocks)
    superblock[68:72] = b"ata1"
    checksum(superblock, 64)
    disk.seek(VOLUME_START)
    disk.write(superblock)
    return app.stat().st_size if app is not None else 0


def read_block(disk, lba):
    disk.seek(VOLUME_START + lba * BLOCK)
    data = disk.read(BLOCK)
    if len(data) != BLOCK:
        raise RuntimeError("lectura corta")
    return data


def file_bytes(disk, ino, offset, length):
    raw = read_block(disk, INODE_TABLE)[ino * 128:(ino + 1) * 128]
    size = struct.unpack_from("<I", raw, 16)[0]
    if offset + length > size:
        raise RuntimeError("lectura fuera del archivo")
    direct = list(struct.unpack_from("<12I", raw, 20))
    indirect = struct.unpack_from("<I", raw, 68)[0]
    double = struct.unpack_from("<I", raw, 88)[0]
    out = bytearray()
    start = offset
    end = offset + length
    while start < end:
        index = start // BLOCK
        if index < 12:
            lba = direct[index]
        elif index < 12 + PTRS:
            table = read_block(disk, indirect)
            lba = struct.unpack_from("<I", table, (index - 12) * 4)[0]
        else:
            rel = index - 12 - PTRS
            top = read_block(disk, double)
            leaf = struct.unpack_from("<I", top, (rel // PTRS) * 4)[0]
            table = read_block(disk, leaf)
            lba = struct.unpack_from("<I", table, (rel % PTRS) * 4)[0]
        block = read_block(disk, lba)
        take = min(BLOCK - (start % BLOCK), end - start)
        out += block[start % BLOCK:start % BLOCK + take]
        start += take
    return bytes(out)


def verify(path, app, size):
    with path.open("rb") as disk:
        magic = disk.read(24)
        if not magic.startswith(b"MettaOS data volume v1"):
            raise RuntimeError("falta la etiqueta ATA")
        if app is None:
            return
        head = file_bytes(disk, 5, 0, 4)
        mark = file_bytes(disk, 5, 0x2F4C0, 4)
        with app.open("rb") as src:
            expect = src.read(4)
            src.seek(0x2F4C0)
            expect_mark = src.read(4)
        if head != expect or mark != expect_mark or head != b"\x7fELF" or mark != b"hsqs":
            raise RuntimeError("el AppImage del disco no coincide")
        if size != app.stat().st_size:
            raise RuntimeError("tamaño distinto")


def stamp_for(app):
    if app is None:
        return "empty"
    st = app.stat()
    return f"{app.resolve()} {st.st_size} {st.st_mtime_ns}"


def main(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    app = find_appimage()
    stamp_path = path.with_name(path.name + ".stamp")
    want = stamp_for(app)
    if path.exists() and stamp_path.is_file() and stamp_path.read_text(encoding="ascii") == want:
        return
    if path.exists() and app is None:
        return
    if path.exists() and app is not None:
        print(f"recreando {path} con {app.name}", file=sys.stderr)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as disk:
        temp = Path(disk.name)
        try:
            size = format_image(disk, app)
            disk.flush()
            os.fsync(disk.fileno())
        except BaseException:
            disk.close()
            temp.unlink(missing_ok=True)
            raise
    try:
        verify(temp, app, size)
        os.replace(temp, path)
        stamp_path.write_text(want, encoding="ascii")
    except BaseException:
        temp.unlink(missing_ok=True)
        raise


if __name__ == "__main__":
    main(Path(sys.argv[1]))

"""Same output as compile_brand.py, using only the Python standard library."""
import struct
import zlib
from pathlib import Path

root = Path(__file__).resolve().parents[2]
names = ("brand", "files", "notes", "terminal", "calculator", "settings", "trash", "package", "network")


def rows(values):
    return ",\n".join(",".join(values[i:i + 16]) for i in range(0, len(values), 16))


def decode_png(path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("not a png: " + str(path))
    pos = 8
    width = height = 0
    color = depth = interlace = 0
    idat = bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
        elif kind == b"IDAT":
            idat += chunk
        elif kind == b"IEND":
            break
    if depth != 8 or color != 6 or interlace != 0:
        raise SystemExit("unsupported png %s depth=%s color=%s" % (path, depth, color))
    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    out = bytearray()
    prev = bytearray(stride)
    i = 0
    for _y in range(height):
        filt = raw[i]
        i += 1
        row = bytearray(raw[i:i + stride])
        i += stride

        def left(x):
            return row[x - 4] if x >= 4 else 0

        def up(x):
            return prev[x]

        def ul(x):
            return prev[x - 4] if x >= 4 else 0

        if filt == 1:
            for x in range(stride):
                row[x] = (row[x] + left(x)) & 255
        elif filt == 2:
            for x in range(stride):
                row[x] = (row[x] + up(x)) & 255
        elif filt == 3:
            for x in range(stride):
                row[x] = (row[x] + ((left(x) + up(x)) // 2)) & 255
        elif filt == 4:
            for x in range(stride):
                a, b, c = left(x), up(x), ul(x)
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
                row[x] = (row[x] + pr) & 255
        elif filt != 0:
            raise SystemExit("bad filter %s in %s" % (filt, path))
        out += row
        prev = row
    if width != 72 or height != 72:
        raise SystemExit("size %s %s" % (width, height))
    pixels = [(out[n], out[n + 1], out[n + 2], out[n + 3]) for n in range(0, len(out), 4)]
    return pixels


tiles = []
last = None
for name in names:
    pixels = decode_png(root / "assets/icons" / (name + ".png"))
    last = pixels
    colors = ["0x%04x" % ((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)) for r, g, b, a in pixels]
    tiles.append("{\n" + rows(colors) + "\n}")
alpha = rows([str(a) for r, g, b, a in last])
(root / "kernel/brand_tiles.c").write_text(
    "#include <brand_tiles.h>\nconst uint16_t brand_tiles[9][BRAND_SIDE*BRAND_SIDE]={\n"
    + ",\n".join(tiles)
    + "\n};\nconst uint8_t brand_alpha[BRAND_SIDE*BRAND_SIDE]={\n"
    + alpha
    + "\n};\n"
)

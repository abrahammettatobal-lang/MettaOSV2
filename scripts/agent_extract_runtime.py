"""Copia solo el ELF del AppImage, hasta el superbloque SquashFS. No el payload."""
import sys

src, dst = sys.argv[1], sys.argv[2]
with open(src, "rb") as f:
    data = f.read(1024 * 1024)
hit = data.find(b"hsqs", 0x2F000)
if hit < 0:
    raise SystemExit("no hay squashfs en el primer megabyte de " + src)
with open(dst, "wb") as out:
    out.write(data[:hit])
print(f"{dst}: {hit} bytes")

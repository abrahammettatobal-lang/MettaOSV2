import struct
import sys

path = sys.argv[1]
with open(path, "rb") as f:
    total = f.seek(0, 2) or f.tell()
    f.seek(0)
    head = f.read(4096)
    ident, typ, mach, ver, entry, phoff, shoff, flags, ehsize, phentsize, phnum, shentsize, shnum, shstr = struct.unpack_from(
        "<16sHHIQQQIHHHHHH", head
    )
    print(f"size={total} shoff={shoff:#x} shnum={shnum}")
    # Scan 4 KiB-aligned offsets for the SquashFS magic, skipping the ELF loads.
    f.seek(0)
    step = 1024 * 1024
    hit = None
    off = 0
    while off < total:
        f.seek(off)
        buf = f.read(step + 4)
        if not buf:
            break
        start = 0
        while True:
            i = buf.find(b"hsqs", start)
            if i < 0:
                break
            abs_off = off + i
            if abs_off >= 0x2f000:
                print(f"hsqs at {abs_off:#x} align={abs_off & 0xFFF:#x}")
                hit = abs_off
                break
            start = i + 1
        if hit is not None:
            break
        off += step
    print("aligned hsqs", hex(hit) if hit is not None else "none")
    # DT_NEEDED from the first DYNAMIC phdr.
    dyn_off = dyn_sz = 0
    str_off = str_sz = 0
    for i in range(phnum):
        off = phoff + i * phentsize
        t, fl, o, va, pa, fsz, msz, al = struct.unpack_from("<IIQQQQQQ", head, off)
        if t == 2:
            dyn_off, dyn_sz = o, fsz
    f.seek(0)
    blob = f.read(max(dyn_off + dyn_sz, 0x30000))
    needed = []
    strtab_va = 0
    for j in range(0, dyn_sz, 16):
        tag, val = struct.unpack_from("<QQ", blob, dyn_off + j)
        if tag == 5:
            strtab_va = val
        elif tag == 1:
            needed.append(val)
        elif tag == 0:
            break
    # strtab file offset: find the LOAD that contains strtab_va
    str_file = None
    f.seek(0)
    hdr = f.read(4096)
    for i in range(phnum):
        off = phoff + i * phentsize
        t, fl, o, va, pa, fsz, msz, al = struct.unpack_from("<IIQQQQQQ", hdr, off)
        if t == 1 and va <= strtab_va < va + fsz:
            str_file = o + (strtab_va - va)
    print("strtab file", hex(str_file) if str_file else "none")
    if str_file is not None:
        for rel in needed:
            end = blob.index(b"\0", str_file + rel)
            print("NEEDED", blob[str_file + rel:end].decode())

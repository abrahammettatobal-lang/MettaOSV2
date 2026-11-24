#!/usr/bin/env python3
"""Copia el cierre de GTK 3 del host y genera el blob que ld.so abre por soname."""
import os
import subprocess
import sys

ROOTS = (
    "/lib/x86_64-linux-gnu/libgtk-3.so.0",
    "/lib/x86_64-linux-gnu/libgdk-3.so.0",
    "/lib/x86_64-linux-gnu/libresolv.so.2",
    "/lib/x86_64-linux-gnu/libnss_files.so.2",
    "/lib/x86_64-linux-gnu/libnss_dns.so.2",
    "/lib/x86_64-linux-gnu/libutil.so.1",
    "/lib/x86_64-linux-gnu/libnsl.so.1",
    "/lib/x86_64-linux-gnu/libanl.so.1",
    "/lib/x86_64-linux-gnu/libasound.so.2",
    "/lib/x86_64-linux-gnu/libX11-xcb.so.1",
)
SKIP = (
    "libc.so",
    "libm.so",
    "libdl.so",
    "libpthread.so",
    "librt.so",
    "libgcc_s.so",
    "libstdc++.so",
    "libz.so",
    "ld-linux",
)
SEARCH = ("/lib/x86_64-linux-gnu/", "/usr/lib/x86_64-linux-gnu/")


def soname_of(path):
    out = subprocess.check_output(["readelf", "-d", path], text=True, errors="replace")
    for line in out.splitlines():
        if "SONAME" in line and "[" in line:
            return line.split("[", 1)[1].split("]", 1)[0]
    return os.path.basename(path)


def needed_of(path):
    out = subprocess.check_output(["readelf", "-d", path], text=True, errors="replace")
    libs = []
    for line in out.splitlines():
        if "NEEDED" not in line or "[" not in line:
            continue
        libs.append(line.split("[", 1)[1].split("]", 1)[0])
    return libs


def resolve(soname):
    for base in SEARCH:
        cand = base + soname
        if os.path.exists(cand):
            return os.path.realpath(cand)
    return None


def collect():
    found = []
    seen = set()
    stack = [os.path.realpath(p) for p in ROOTS if os.path.exists(p)]
    while stack:
        path = stack.pop()
        if path in seen:
            continue
        seen.add(path)
        name = soname_of(path)
        if not name.startswith(SKIP):
            found.append((name, path))
        for dep in needed_of(path):
            if dep.startswith(SKIP):
                continue
            nxt = resolve(dep)
            if nxt is not None:
                stack.append(nxt)
    found.sort(key=lambda item: item[0])
    return found


def main():
    if len(sys.argv) != 3:
        print("uso: gen_hostlibs.py hostlibs.S hostlibs.gen.c", file=sys.stderr)
        return 1
    out_s, out_c = sys.argv[1], sys.argv[2]
    host = os.path.join(os.path.dirname(out_s), "host")
    os.makedirs(host, exist_ok=True)
    libs = collect()
    s_lines = [
        "; Generado por scripts/gen_hostlibs.py. No editar.",
        "",
    ]
    c_lines = [
        "/* Generado por scripts/gen_hostlibs.py. No editar. */",
        "#include <linux/user_files.h>",
        "#include <stdint.h>",
        "",
    ]
    for i, (soname, path) in enumerate(libs):
        dest = os.path.join(host, soname)
        data = open(path, "rb").read()
        with open(dest, "wb") as fh:
            fh.write(data)
        s_lines.append("align 16")
        s_lines.append(f"global hostlib_{i}_start")
        s_lines.append(f"global hostlib_{i}_end")
        s_lines.append(f"hostlib_{i}_start:")
        s_lines.append(f'incbin "{dest}"')
        s_lines.append(f"hostlib_{i}_end:")
        s_lines.append("")
        c_lines.append(f"extern const uint8_t hostlib_{i}_start[];")
        c_lines.append(f"extern const uint8_t hostlib_{i}_end[];")
    c_lines.append("")
    c_lines.append("void linux_user_register_hostlibs(void)")
    c_lines.append("{")
    for i, (soname, _path) in enumerate(libs):
        for prefix in ("/lib/x86_64-linux-gnu/", "/usr/lib/x86_64-linux-gnu/"):
            c_lines.append(
                f'    linux_user_hostlib_add("{prefix}{soname}", '
                f"hostlib_{i}_start, hostlib_{i}_end);"
            )
    c_lines.append("}")
    c_lines.append("")
    with open(out_s, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(s_lines))
    with open(out_c, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(c_lines))
    print(f"hostlibs: {len(libs)} bibliotecas")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

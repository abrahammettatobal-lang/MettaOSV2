#!/usr/bin/env python3
"""Regenera wallpaper Tahoe LG-T06 e iconos dock (PNG + headers). Uso local.

Prompts IA (PNG sRGB, fondo transparente) si se reemplazan rasteres:
- Notes 40x40: "Minimal macOS Tahoe app icon, yellow legal notepad with folded corner,
  soft gradient #FFF6CC to #FFE082, subtle inner shadow, rounded square, premium flat 3D"
- Files 40x40: "Tahoe style folder icon, sky blue #4A9EF5 with tab, glass highlight top,
  soft drop shadow, rounded corners, MDE Light palette"
- Terminal 40x40: "Dark charcoal rounded terminal #2A2E36, traffic lights red/yellow/green,
  green monospace prompt chevron, macOS Tahoe dock icon"
- Calculator 40x40: "Light calculator icon, white face #FFFFFF, blue accent keys #3278D9,
  gray operators #9AA8B8, rounded shell, Tahoe liquid-glass subtle gloss"
Export 40x40 and 32x32, then re-run this script or embed_rgba pipeline.
"""
import math
import os
import struct
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
W, H = 192, 108


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def lerp(a, b, t):
    return a + (b - a) * t


def pearl_base(x, y):
    t = y / (H - 1)
    r = lerp(0xFA, 0xDC, t)
    g = lerp(0xFC, 0xE8, t)
    b = lerp(0xFF, 0xF8, t)
    hx = math.sin((x / max(W - 1, 1)) * math.pi * 2.0)
    r = lerp(r, 0xE4, 0.10 * hx)
    g = lerp(g, 0xEC, 0.08 * math.sin(x * 0.04 + 1.2))
    b = lerp(b, 0xFF, 0.12 * math.sin(x * 0.05 + 0.6))
    r = lerp(r, 0xF0, 0.06 * math.sin(y * 0.11))
    g = lerp(g, 0xF4, 0.05 * math.sin(y * 0.09 + 0.8))
    b = lerp(b, 0xFC, 0.07 * math.sin(y * 0.08 + 1.4))
    return r, g, b


def blob(x, y, cx, cy, rx, ry, cr, cg, cb, alpha):
    nx = (x - cx) / rx
    ny = (y - cy) / ry
    d = nx * nx + ny * ny
    if d > 1.0:
        return 0, 0, 0, 0
    fall = (1.0 - d) ** 2
    return cr, cg, cb, fall * alpha


def make_wallpaper():
    blobs = [
        (42, 22, 72, 52, 0x5A, 0xAE, 0xF5, 0.62),
        (128, 18, 78, 58, 0x7A, 0xC4, 0xFF, 0.50),
        (92, 52, 64, 44, 0x8E, 0xD0, 0xFF, 0.45),
        (162, 42, 58, 48, 0xA8, 0xDC, 0xFF, 0.40),
        (24, 68, 54, 38, 0x98, 0xCC, 0xF5, 0.34),
        (150, 82, 46, 34, 0xC0, 0xE4, 0xFF, 0.30),
        (72, 88, 40, 28, 0x6A, 0xB8, 0xF0, 0.22),
    ]
    pixels = []
    for y in range(H):
        row = []
        for x in range(W):
            br, bg, bb = pearl_base(x, y)
            ar = ag = ab = 0
            aa = 0.0
            for cx, cy, rx, ry, cr, cg, cb, al in blobs:
                r, g, b, a = blob(x, y, cx, cy, rx, ry, cr, cg, cb, al)
                if a > aa:
                    ar, ag, ab, aa = r, g, b, a
            t = aa
            r = int(br * (1 - t) + ar * t)
            g = int(bg * (1 - t) + ag * t)
            b = int(bb * (1 - t) + ab * t)
            streak = (
                math.exp(-((y - 24) ** 2) / 160.0)
                * math.exp(-((x - 112) ** 2) / 720.0)
                * 0.32
            )
            haze = 0.12 * math.exp(-((y - 72) ** 2) / 420.0)
            r = clamp(int(r + 32 * streak + 18 * haze), 0, 255)
            g = clamp(int(g + 38 * streak + 22 * haze), 0, 255)
            b = clamp(int(b + 48 * streak + 30 * haze), 0, 255)
            row.append((r, g, b, 255))
        pixels.append(row)
    return pixels


def write_png(path, px, w, h):
    raw = b"".join(bytes([0] + list(px[y][x][:3])) for y in range(h) for x in range(w))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)

    def chunk(t, d):
        return (
            struct.pack(">I", len(d))
            + t
            + d
            + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
        )

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDATA", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def write_wallpaper_h(path, px, w, h):
    lines = [
        "/* MettaOS — wallpaper Tahoe Liquid Glass (LG-T06 / LG-07) */",
        "/* Fuente: assets/wallpaper/light_default.png — NO sustituir logo metálico oficial */",
        "#ifndef METTAOS_ASSETS_WALLPAPER_LIGHT_DEFAULT_H",
        "#define METTAOS_ASSETS_WALLPAPER_LIGHT_DEFAULT_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define WALLPAPER_LIGHT_DEFAULT_WIDTH  {w}u",
        f"#define WALLPAPER_LIGHT_DEFAULT_HEIGHT {h}u",
        "#define WALLPAPER_LIGHT_DEFAULT_FORMAT 0 /* 0 = RGBA8888 */",
        "",
        "static const uint32_t wallpaper_light_default_rgba["
        "WALLPAPER_LIGHT_DEFAULT_WIDTH * WALLPAPER_LIGHT_DEFAULT_HEIGHT] = {",
    ]
    for y in range(h):
        vals = [f"0x{r:02x}{g:02x}{b:02x}{a:02x}u" for r, g, b, a in px[y]]
        lines.append("    " + ", ".join(vals) + ("," if y < h - 1 else ""))
    lines += [
        "};",
        "",
        "#endif /* METTAOS_ASSETS_WALLPAPER_LIGHT_DEFAULT_H */",
        "",
    ]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def dist(x, y, cx, cy):
    return math.hypot(x - cx, y - cy)


def rounded_rect(px, x0, y0, x1, y1, rad, inside_fn):
    h = len(px)
    w = len(px[0])
    for y in range(h):
        for x in range(w):
            if x0 <= x <= x1 and y0 <= y <= y1:
                cx, cy = x, y
                if x < x0 + rad and y < y0 + rad:
                    cx, cy = x0 + rad, y0 + rad
                elif x > x1 - rad and y < y0 + rad:
                    cx, cy = x1 - rad, y0 + rad
                elif x < x0 + rad and y > y1 - rad:
                    cx, cy = x0 + rad, y1 - rad
                elif x > x1 - rad and y > y1 - rad:
                    cx, cy = x1 - rad, y1 - rad
                if x < x0 + rad or x > x1 - rad or y < y0 + rad or y > y1 - rad:
                    if dist(x, y, cx, cy) > rad:
                        continue
                inside_fn(px, x, y)


def new_icon(w, h):
    return [[[0, 0, 0, 0] for _ in range(w)] for _ in range(h)]


def blend(dst, r, g, b, a):
    if a <= 0:
        return
    dr, dg, db, da = dst
    if da == 0:
        dst[0], dst[1], dst[2], dst[3] = r, g, b, a
        return
    t = a / 255.0
    dst[0] = int(dr * (1 - t) + r * t)
    dst[1] = int(dg * (1 - t) + g * t)
    dst[2] = int(db * (1 - t) + b * t)
    dst[3] = min(255, int(da + a * (1 - da / 255.0)))


def icon_files(sz):
    px = new_icon(sz, sz)

    def paint(_, x, y, c):
        blend(px[y][x], *c)

    rad = max(3, sz // 8)
    body_y0 = sz // 3
    body_x0 = sz // 6
    body_x1 = sz - sz // 6 - 1
    body_y1 = sz - sz // 6 - 1

    def body(_, x, y):
        paint(px, x, y, (0x4A, 0x9E, 0xF5, 255))

    rounded_rect(px, body_x0, body_y0, body_x1, body_y1, rad, body)
    tab_x1 = body_x0 + sz // 3
    tab_y1 = body_y0 - 1
    tab_y0 = sz // 5
    for y in range(tab_y0, tab_y1 + 1):
        for x in range(body_x0, tab_x1):
            if y == tab_y0 or x == body_x0 or y == tab_y1:
                paint(px, x, y, (0x6A, 0xB8, 0xF8, 255))
            else:
                paint(px, x, y, (0x8E, 0xCF, 0xFF, 255))
    for y in range(body_y0 + 2, body_y0 + max(3, sz // 10)):
        for x in range(body_x0 + 2, body_x1 - 2):
            paint(px, x, y, (0xA8, 0xDA, 0xFF, 90))
    return px


def icon_notes(sz):
    px = new_icon(sz, sz)
    rad = max(3, sz // 10)
    x0, y0 = sz // 4, sz // 6
    x1, y1 = sz - sz // 4, sz - sz // 6

    def page(_, x, y):
        blend(px[y][x], 0xFF, 0xFF, 0xFF, 250)
        if x == x0 or y == y0 or x == x1 or y == y1:
            blend(px[y][x], 0xCB, 0xD5, 0xE0, 255)

    rounded_rect(px, x0, y0, x1, y1, rad, page)
    fold = sz // 6
    for i in range(fold):
        for j in range(i + 1):
            x, y = x1 - i, y0 + j
            if 0 <= x < sz and 0 <= y < sz:
                blend(px[y][x], 0xE8, 0xED, 0xF2, 255)
            x, y = x1 - j, y0 + i
            if 0 <= x < sz and 0 <= y < sz:
                blend(px[y][x], 0xF6, 0xF8, 0xFB, 255)
    for li, yy in enumerate(range(y0 + sz // 5, y1 - sz // 6, max(4, sz // 8))):
        color = (0x9A, 0xA8, 0xB8, 200) if li > 0 else (0x32, 0x78, 0xD9, 220)
        for x in range(x0 + sz // 8, x1 - sz // 5):
            blend(px[yy][x], *color)
    return px


def icon_calculator(sz):
    px = new_icon(sz, sz)
    rad = max(3, sz // 8)
    x0, y0 = sz // 6, sz // 6
    x1, y1 = sz - sz // 6 - 1, sz - sz // 6 - 1

    def face(_, x, y):
        blend(px[y][x], 0xF8, 0xFA, 0xFC, 252)
        if x <= x0 + 1 or y <= y0 + 1:
            blend(px[y][x], 0xCB, 0xD5, 0xE0, 220)

    rounded_rect(px, x0, y0, x1, y1, rad, face)
    disp_y1 = y0 + max(6, sz // 5)
    for y in range(y0 + 2, disp_y1):
        for x in range(x0 + 3, x1 - 3):
            blend(px[y][x], 0xEA, 0xF0, 0xF6, 255)
    blend(px[disp_y1 + 1][x0 + 4], 0x32, 0x78, 0xD9, 240)
    for x in range(x0 + 5, x0 + max(10, sz // 4)):
        blend(px[disp_y1 + 1][x], 0x2D, 0x64, 0xC0, 220)
    grid_y0 = disp_y1 + max(4, sz // 10)
    cell = max(3, sz // 9)
    keys = [
        (0, 0, 0xE8, 0xED, 0xF2),
        (1, 0, 0xE8, 0xED, 0xF2),
        (2, 0, 0x9A, 0xA8, 0xB8),
        (0, 1, 0xE8, 0xED, 0xF2),
        (1, 1, 0xE8, 0xED, 0xF2),
        (2, 1, 0x32, 0x78, 0xD9),
        (0, 2, 0xE8, 0xED, 0xF2),
        (1, 2, 0x32, 0x78, 0xD9),
        (2, 2, 0x32, 0x78, 0xD9),
    ]
    for kx, ky, cr, cg, cb in keys:
        bx = x0 + sz // 8 + kx * (cell + 1)
        by = grid_y0 + ky * (cell + 1)
        for y in range(by, min(by + cell, y1 - 1)):
            for x in range(bx, min(bx + cell, x1 - 1)):
                blend(px[y][x], cr, cg, cb, 255)
    return px


def icon_terminal(sz):
    px = new_icon(sz, sz)
    rad = max(4, sz // 7)
    x0, y0 = sz // 7, sz // 7
    x1, y1 = sz - sz // 7 - 1, sz - sz // 7 - 1

    def shell(_, x, y):
        blend(px[y][x], 0x2A, 0x2E, 0x36, 245)
        if x <= x0 + 1 or y <= y0 + 1:
            blend(px[y][x], 0x45, 0x4A, 0x55, 255)

    rounded_rect(px, x0, y0, x1, y1, rad, shell)
    for i, col in enumerate([(0xF2, 0x6A, 0x6A), (0xF5, 0xC0, 0x51), (0x6B, 0xD4, 0x8E)]):
        cx = x0 + sz // 9 + i * max(4, sz // 12)
        cy = y0 + sz // 10
        for y in range(cy - 1, cy + 2):
            for x in range(cx - 1, cx + 2):
                if 0 <= x < sz and 0 <= y < sz:
                    blend(px[y][x], *col, 255)
    base_y = y0 + sz // 3
    for i in range(max(5, sz // 6)):
        x = x0 + sz // 8 + i
        y1_ = base_y + i // 2
        y2_ = base_y + max(3, sz // 8) - i // 2
        for y in (y1_, y2_):
            if 0 <= x < sz and 0 <= y < sz:
                blend(px[y][x], 0x6B, 0xD4, 0x8E, 255)
    uy = base_y + max(5, sz // 7)
    for x in range(x0 + sz // 3, x0 + sz // 3 + max(6, sz // 5)):
        if 0 <= x < sz:
            blend(px[uy][x], 0xC8, 0xD4, 0xDE, 220)
    return px


def emit_icon(lines, prefix, px, w, h):
    lines.append(f"#define {prefix}_WIDTH  {w}u")
    lines.append(f"#define {prefix}_HEIGHT {h}u")
    lines.append(f"static const uint32_t {prefix.lower()}_rgba[{w} * {h}] = {{")
    for y in range(h):
        vals = [f"0x{r:02x}{g:02x}{b:02x}{a:02x}u" for r, g, b, a in px[y]]
        lines.append("    " + ", ".join(vals) + ("," if y < h - 1 else ""))
    lines.append("};")
    lines.append("")


def main():
    wp = make_wallpaper()
    wp_png = os.path.join(ROOT, "assets", "wallpaper", "light_default.png")
    wp_h = os.path.join(ROOT, "assets", "wallpaper", "light_default.h")
    write_png(wp_png, wp, W, H)
    write_wallpaper_h(wp_h, wp, W, H)

    icons = {
        "files": icon_files,
        "notes": icon_notes,
        "terminal": icon_terminal,
        "calculator": icon_calculator,
    }
    icons_dir = os.path.join(ROOT, "assets", "icons")
    for name, fn in icons.items():
        px40 = fn(40)
        px_rgb = [[t[:3] for t in row] for row in px40]
        write_png(os.path.join(icons_dir, f"{name}.png"), px_rgb, 40, 40)

    lines = [
        "/* MettaOS — iconografía dock MDE Light / Tahoe (LG-T06) */",
        "#ifndef METTAOS_ASSETS_ICONS_MDE_H",
        "#define METTAOS_ASSETS_ICONS_MDE_H",
        "",
        "#include <stdint.h>",
        "",
        "#define MDE_ICON_FORMAT_RGBA8888 0u",
        "#define MDE_ICON_DOCK_SIZE       40u",
        "#define MDE_ICON_SIZE            32u",
        "",
    ]
    for name, fn in icons.items():
        emit_icon(lines, f"MDE_ICON_{name.upper()}_40", fn(40), 40, 40)
        emit_icon(lines, f"MDE_ICON_{name.upper()}_32", fn(32), 32, 32)
    lines += [
        "typedef struct mde_icon_asset {",
        "    const char     *app_id;",
        "    const uint32_t *pixels_40;",
        "    const uint32_t *pixels_32;",
        "} mde_icon_asset_t;",
        "",
        "static const mde_icon_asset_t mde_dock_icons[] = {",
        '    { "mde.notes",    mde_icon_notes_40_rgba,    mde_icon_notes_32_rgba },',
        '    { "mde.files",    mde_icon_files_40_rgba,    mde_icon_files_32_rgba },',
        '    { "mde.terminal", mde_icon_terminal_40_rgba, mde_icon_terminal_32_rgba },',
        '    { "mde.calculator", mde_icon_calculator_40_rgba, mde_icon_calculator_32_rgba },',
        "};",
        "",
        "#define MDE_DOCK_ICON_ASSET_COUNT 4u",
        "",
        "#endif /* METTAOS_ASSETS_ICONS_MDE_H */",
        "",
    ]
    out_h = os.path.join(icons_dir, "icons_mde.h")
    with open(out_h, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("OK wallpaper", W, H, "icons", len(icons))


if __name__ == "__main__":
    main()
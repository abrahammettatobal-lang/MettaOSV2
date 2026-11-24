"""Genera mde/ui/ui_font_data.h: glifos alfa 8 bits (antialias) para el shell MettaOS.

Uso: python assets/tools/gen_ui_fonts.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "mde", "ui", "ui_font_data.h")
FONTS_DIR = r"C:\Windows\Fonts"

# Orden = enum ui_font_id en ui.h
SPECS = [
    ("UI", "segoeui.ttf", 14),
    ("UI_BOLD", "seguisb.ttf", 14),
    ("SMALL", "segoeui.ttf", 12),
    ("TITLE", "seguisb.ttf", 20),
    ("LARGE", "segoeuil.ttf", 46),
    ("MONO", "consola.ttf", 14),
    ("HUGE", "segoeuil.ttf", 64),
]

EXTRA = "áéíóúÁÉÍÓÚñÑüÜ¿¡°·…×÷−←→↑↓•✓—–«»±€çÇ´¨¬ºª“”‘’"
CHARS = [chr(c) for c in range(32, 127)] + list(EXTRA)
GAMMA = 0.85


def render_font(path, size):
    font = ImageFont.truetype(path, size)
    ascent, descent = font.getmetrics()
    glyphs = []
    for ch in CHARS:
        adv = int(round(font.getlength(ch)))
        l, t, r, b = font.getbbox(ch)
        w, h = max(0, r - l), max(0, b - t)
        data = b""
        if w > 0 and h > 0:
            im = Image.new("L", (w, h), 0)
            ImageDraw.Draw(im).text((-l, -t), ch, font=font, fill=255)
            px = im.tobytes()
            data = bytes(int(round(255 * ((v / 255.0) ** GAMMA))) for v in px)
        glyphs.append((ch, w, h, l, t, adv, data))
    return ascent, descent, glyphs


def main():
    blob = bytearray()
    out = []
    out.append("/* Generado por assets/tools/gen_ui_fonts.py — no editar a mano. */")
    out.append("#ifndef MDE_UI_FONT_DATA_H")
    out.append("#define MDE_UI_FONT_DATA_H")
    out.append("")
    out.append("#include <stdint.h>")
    out.append("")
    out.append("typedef struct { uint32_t off; uint8_t w, h, adv; int8_t xo, yo; } ui_glyph_t;")
    out.append("typedef struct { const ui_glyph_t *g; uint8_t ascent, line; } ui_font_t;")
    out.append("")
    extra_cps = ", ".join(f"0x{ord(c):04X}" for c in EXTRA)
    out.append(f"#define UI_FONT_EXTRA_COUNT {len(EXTRA)}")
    out.append(f"static const uint16_t ui_font_extra_cp[UI_FONT_EXTRA_COUNT] = {{ {extra_cps} }};")
    out.append(f"#define UI_FONT_GLYPHS {len(CHARS)}")
    out.append("")

    tables = []
    for name, file, size in SPECS:
        ascent, descent, glyphs = render_font(os.path.join(FONTS_DIR, file), size)
        rows = []
        for ch, w, h, xo, yo, adv, data in glyphs:
            rows.append(f"{{{len(blob)},{w},{h},{adv},{xo},{yo}}}")
            blob.extend(data)
        tables.append((name, ascent, ascent + descent, rows))

    out.append(f"static const uint8_t ui_font_alpha[{max(1, len(blob))}] = {{")
    for i in range(0, len(blob), 32):
        out.append("  " + ",".join(str(v) for v in blob[i:i + 32]) + ",")
    out.append("};")
    out.append("")
    for name, ascent, line, rows in tables:
        out.append(f"static const ui_glyph_t ui_glyphs_{name.lower()}[UI_FONT_GLYPHS] = {{")
        for i in range(0, len(rows), 8):
            out.append("  " + ",".join(rows[i:i + 8]) + ",")
        out.append("};")
    out.append("")
    out.append(f"static const ui_font_t ui_fonts[{len(SPECS)}] = {{")
    for name, ascent, line, _ in tables:
        out.append(f"  {{ ui_glyphs_{name.lower()}, {ascent}, {line} }},")
    out.append("};")
    out.append("")
    out.append("#endif")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print(f"OK {OUT}: {len(blob)} bytes alfa, {len(CHARS)} glifos x {len(SPECS)} fuentes")


if __name__ == "__main__":
    main()

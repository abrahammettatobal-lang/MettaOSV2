"""Compile the nine RGBA PNGs into the kernel's RGB565 atlas."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[2]
names = ("brand", "files", "notes", "terminal", "calculator", "settings", "trash", "package", "network")

def rows(values):
    return ",\n".join(",".join(values[i:i+16]) for i in range(0,len(values),16))

tiles = []
for name in names:
    icon = Image.open(root / "assets/icons" / (name+".png")).convert("RGBA")
    assert icon.size == (72, 72)
    colors = ["0x%04x" % ((r>>3)<<11 | (g>>2)<<5 | b>>3) for r,g,b,a in icon.getdata()]
    tiles.append("{\n"+rows(colors)+"\n}")
alpha = rows([str(a) for a in icon.getchannel("A").getdata()])
(root / "kernel/brand_tiles.c").write_text('#include <brand_tiles.h>\nconst uint16_t brand_tiles[9][BRAND_SIDE*BRAND_SIDE]={\n'+',\n'.join(tiles)+'\n};\nconst uint8_t brand_alpha[BRAND_SIDE*BRAND_SIDE]={\n'+alpha+'\n};\n')

#include "ui_internal.h"
#include <brand_tiles.h>

static void draw_tile(ui_canvas_t *c, int tile, float x, float y, float size)
{
    int left = (int)(x + 0.5f), top = (int)(y + 0.5f), side = (int)(size + 0.5f);
    if (side <= 0)
        return;
    for (int py = ui_max(top, c->cy0); py < ui_min(top + side, c->cy1); py++) {
        int row = (py - top) * BRAND_SIDE / side * BRAND_SIDE;
        for (int px = ui_max(left, c->cx0); px < ui_min(left + side, c->cx1); px++) {
            int i = row + (px - left) * BRAND_SIDE / side;
            uint32_t *dest = &c->px[(int64_t)py * c->stride + px];
            *dest = ui_mix(*dest, brand_color(tile, i), brand_alpha[i]);
        }
    }
}
void ui_logo_draw(ui_canvas_t *c, float cx, float cy, float s, uint32_t rgb, uint32_t a)
{
    (void)rgb;
    (void)a;
    draw_tile(c, 0, cx - s / 2, cy - s / 2, s);
}
void ui_icon_draw(ui_canvas_t *c, int icon, float x, float y, float s)
{
    static const uint8_t tiles[] = {
        [UI_ICON_FILES] = 1,    [UI_ICON_NOTES] = 2, [UI_ICON_TERMINAL] = 3, [UI_ICON_CALC] = 4,
        [UI_ICON_SETTINGS] = 5, [UI_ICON_TRASH] = 6, [UI_ICON_FOLDER] = 1,   [UI_ICON_DOC] = 2,
        [UI_ICON_APPIMAGE] = 7, [UI_ICON_ABOUT] = 0, [UI_ICON_NETWORK] = 8};
    draw_tile(c, (unsigned)icon < sizeof(tiles) ? tiles[icon] : 0, x, y, s);
}

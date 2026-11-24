#ifndef METTA_BRAND_TILES_H
#define METTA_BRAND_TILES_H
#include <stdint.h>
#define BRAND_SIDE 72
extern const uint16_t brand_tiles[9][BRAND_SIDE * BRAND_SIDE];
extern const uint8_t brand_alpha[BRAND_SIDE * BRAND_SIDE];
static inline uint32_t brand_color(int tile, int pixel)
{
    uint16_t v = brand_tiles[tile][pixel];
    unsigned r = v >> 11, g = (v >> 5) & 63, b = v & 31;
    return ((r * 255 / 31) << 16) | ((g * 255 / 63) << 8) | (b * 255 / 31);
}
#endif

/*
 * MettaOS shell — fondos procedurales a resolución nativa (capas de ondas tipo Tahoe).
 */
#include "ui_internal.h"

typedef struct {
    const char *name;
    uint32_t sky_top, sky_bot;
    uint32_t la[3], lb[3];  /* color superior/inferior de cada capa */
    float    y[3], amp[3], freq[3], phase[3];
    uint32_t sheen;
    uint32_t sheen_a;
    float    sun_x, sun_y, sun_r;
    uint32_t sun_a;
} wall_def_t;

static const wall_def_t k_walls[UI_WALL_COUNT] = {
    { "Sierra", 0xDDEFFFu, 0x9FCBF6u,
      { 0x86BEF4u, 0x3277DCu, 0x173F9Eu }, { 0x4A90E2u, 0x1B4FB8u, 0x0B2466u },
      { 0.50f, 0.65f, 0.81f }, { 0.07f, 0.06f, 0.05f }, { 0.85f, 1.25f, 0.70f },
      { 0.4f, 2.1f, 4.0f }, 0xFFFFFFu, 150u, 0.78f, 0.22f, 0.45f, 120u },
    { "Tahoe Noche", 0x0A1030u, 0x261F5Cu,
      { 0x3B2D8Eu, 0x1C5CB6u, 0x0F2152u }, { 0x281E6Cu, 0x113A84u, 0x070F2Cu },
      { 0.52f, 0.67f, 0.83f }, { 0.07f, 0.06f, 0.05f }, { 0.80f, 1.20f, 0.75f },
      { 1.1f, 3.0f, 5.2f }, 0x7FD8FFu, 110u, 0.22f, 0.18f, 0.40f, 60u },
    { "Atardecer", 0xFFE6C8u, 0xFFB5A2u,
      { 0xFF8C80u, 0xC94B8Cu, 0x5A2E7Au }, { 0xF0607Eu, 0x9B3A86u, 0x341A55u },
      { 0.50f, 0.66f, 0.82f }, { 0.08f, 0.06f, 0.05f }, { 0.90f, 1.30f, 0.65f },
      { 2.0f, 0.6f, 3.3f }, 0xFFF4E0u, 140u, 0.30f, 0.30f, 0.40f, 110u },
    { "Grafito", 0x3C424Bu, 0x2A2F36u,
      { 0x4C535Du, 0x30353Cu, 0x1D2025u }, { 0x3A3F47u, 0x24282Eu, 0x121417u },
      { 0.52f, 0.67f, 0.83f }, { 0.06f, 0.05f, 0.04f }, { 0.80f, 1.10f, 0.70f },
      { 0.9f, 2.5f, 4.4f }, 0xC8D2E0u, 70u, 0.70f, 0.20f, 0.40f, 40u },
};

const char *ui_wall_name(int idx)
{
    return k_walls[ui_clampi(idx, 0, UI_WALL_COUNT - 1)].name;
}

#define WALL_MAX_COLS 1024

static float hill_noise(unsigned x, unsigned key)
{
    unsigned n = x * 747796405u + key * 2891336453u;
    n = ((n >> ((n >> 28) + 4)) ^ n) * 277803737u;
    return (float)((n ^ (n >> 22)) & 65535u) / 65535.0f - .5f;
}

static float hills(float x, unsigned key)
{
    int i = (int)x;
    float t = x - (float)i;
    t = t*t*(3.0f-2.0f*t);
    float a = hill_noise((unsigned)i, key);
    return a + (hill_noise((unsigned)i+1, key)-a)*t;
}

static void sierra(uint32_t *dst, int w, int h, int stride)
{
    static const uint32_t ridge[] = {0xC4CDC0,0xABBCAD,0x8BA99C,0x638C82,0x406F68,0x285552,0x183F40};
    static const uint32_t shade[] = {0xD3D6C3,0xC0CCB9,0xABC3B1,0x86A998,0x6D9182,0x49796D,0x285653};
    static int edge[7][WALL_MAX_COLS];
    for (int k=0;k<7;k++) {
        for (int x=0;x<w;x++) {
            float u = (float)x/w, seed = 1.8f + k*.31f;
            float height = .44f + k*.068f + hills(u*4.0f+seed, 44+k)*.19f
                         + hills(u*12.0f+seed, 17+k)*.077f
                         + hills(u*34.0f+seed, 51+k)*.027f
                         + hills(u*98.0f, 93+k)*.01f;
            edge[k][x] = (int)(height*h);
        }
    }
    int sx=w*29/100, sy=h*31/100, radius=ui_max(2,h/21);
    for (int y=0;y<h;y++) {
        uint32_t sky=ui_mix(0x9BC5C5, 0xF9E4BC, (unsigned)ui_min(255,y*440/h));
        for (int x=0;x<w;x++) {
            uint32_t p=sky;
            int dx=x-sx, dy=y-sy, d=dx*dx+dy*dy, r2=radius*radius;
            if (d < r2*16) p=ui_mix(p,0xFFEFCC,(unsigned)((r2*16-d)*72/(r2*16)));
            if (d < r2+radius) p=ui_mix(p,0xFFF3D6,(unsigned)ui_clampi((r2+radius-d)*255/(2*radius),0,255));
            for (int k=6;k>=0;k--) {
                int depth=y-edge[k][x];
                if (depth<0) continue;
                p=ui_mix(ridge[k],shade[k],(unsigned)ui_min(200,depth*490/h));
                break;
            }
            dst[(int64_t)y*stride+x]=p;
        }
    }
}

void ui_wall_render(int idx, uint32_t *dst, int32_t w, int32_t h, int32_t stride,
                    int32_t full_w, int32_t full_h)
{
    const wall_def_t *d = &k_walls[ui_clampi(idx, 0, UI_WALL_COUNT - 1)];
    static float curve[3][WALL_MAX_COLS];
    int32_t x, y, k;
    float scale = (float)h / 768.0f;

    (void)full_w;
    (void)full_h;
    if (w > WALL_MAX_COLS)
        w = WALL_MAX_COLS;
    if (idx == 0 && w > 0 && h > 0) {
        sierra(dst, w, h, stride);
        return;
    }

    for (k = 0; k < 3; k++) {
        for (x = 0; x < w; x++) {
            float u = (float)x / (float)w;
            curve[k][x] = d->y[k] +
                          d->amp[k] * ui_sinf(u * 6.2831853f * d->freq[k] + d->phase[k]) +
                          d->amp[k] * 0.35f * ui_sinf(u * 6.2831853f * d->freq[k] * 2.3f +
                                                      d->phase[k] * 1.7f);
        }
    }

    for (y = 0; y < h; y++) {
        float v = (float)y / (float)h;
        uint32_t *row = dst + (int64_t)y * stride;
        uint32_t sky = ui_mix(d->sky_top, d->sky_bot, (uint32_t)(v * 255.0f));
        float edge = 1.4f / (float)h; /* ~1.4 px de antialias */

        for (x = 0; x < w; x++) {
            float u = (float)x / (float)w;
            uint32_t col = sky;
            float sdx = (u - d->sun_x) * ((float)w / (float)h);
            float sdy = v - d->sun_y;
            float sd2 = (sdx * sdx + sdy * sdy) / (d->sun_r * d->sun_r);

            if (sd2 < 1.0f) {
                float f = 1.0f - sd2;
                col = ui_mix(col, 0xFFFFFFu, (uint32_t)((float)d->sun_a * f * f));
            }

            for (k = 0; k < 3; k++) {
                float yk = curve[k][x];
                float t = (v - yk) / edge;
                float glow = (v - yk) / (0.010f * scale + 0.004f);

                if (t > -1.0f) {
                    float cov = ui_clampf(0.5f + t * 0.5f, 0.0f, 1.0f);
                    float gt = ui_clampf((v - yk) / 0.35f, 0.0f, 1.0f);
                    uint32_t lc = ui_mix(d->la[k], d->lb[k], (uint32_t)(gt * 255.0f));
                    col = ui_mix(col, lc, (uint32_t)(cov * 255.0f));
                }
                if (glow > -6.0f && glow < 6.0f) {
                    float gl = 1.0f / (1.0f + glow * glow);
                    col = ui_mix(col, d->sheen, (uint32_t)((float)d->sheen_a * gl));
                }
            }

            {
                uint32_t hsh = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u;
                int32_t n = (int32_t)((hsh >> 13) & 3u) - 1;
                int32_t r = (int32_t)((col >> 16) & 0xFFu) + n;
                int32_t g = (int32_t)((col >> 8) & 0xFFu) + n;
                int32_t b = (int32_t)(col & 0xFFu) + n;
                row[x] = UI_RGB(ui_clampi(r, 0, 255), ui_clampi(g, 0, 255), ui_clampi(b, 0, 255));
            }
        }
    }
}

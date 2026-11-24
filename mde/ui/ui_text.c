/*
 * MettaOS shell — texto antialias (Segoe UI / Consolas rasterizadas) + utilidades.
 */
#include "ui_internal.h"
#include "ui_font_data.h"

#include <interrupt/ports.h>

uint32_t ui_utf8_next(const char *s, int32_t *i)
{
    const uint8_t *p = (const uint8_t *)s + *i;
    uint32_t cp;

    if (p[0] < 0x80u) {
        *i += 1;
        return p[0];
    }
    if ((p[0] & 0xE0u) == 0xC0u && p[1]) {
        cp = ((uint32_t)(p[0] & 0x1Fu) << 6) | (p[1] & 0x3Fu);
        *i += 2;
        return cp;
    }
    if ((p[0] & 0xF0u) == 0xE0u && p[1] && p[2]) {
        cp = ((uint32_t)(p[0] & 0x0Fu) << 12) | ((uint32_t)(p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
        *i += 3;
        return cp;
    }
    *i += 1;
    return '?';
}

int32_t ui_utf8_prev(const char *s, int32_t i)
{
    if (i <= 0)
        return 0;
    i--;
    while (i > 0 && (((uint8_t)s[i]) & 0xC0u) == 0x80u)
        i--;
    return i;
}

int32_t ui_utf8_encode(uint32_t cp, char *out)
{
    if (cp < 0x80u) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    out[0] = (char)(0xE0u | (cp >> 12));
    out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[2] = (char)(0x80u | (cp & 0x3Fu));
    return 3;
}

static int glyph_index(uint32_t cp)
{
    uint32_t i;

    if (cp >= 32u && cp <= 126u)
        return (int)(cp - 32u);
    for (i = 0; i < UI_FONT_EXTRA_COUNT; i++) {
        if (ui_font_extra_cp[i] == cp)
            return (int)(95u + i);
    }
    return (int)('?' - 32);
}

int32_t ui_font_line(int font)
{
    return ui_fonts[font].line;
}

int32_t ui_font_ascent(int font)
{
    return ui_fonts[font].ascent;
}

static void draw_glyph(ui_canvas_t *c, int32_t x, int32_t y, const ui_glyph_t *g,
                       uint32_t rgb, uint32_t a)
{
    int32_t gx0 = x + g->xo, gy0 = y + g->yo;
    int32_t x0 = ui_max(gx0, c->cx0), y0 = ui_max(gy0, c->cy0);
    int32_t x1 = ui_min(gx0 + g->w, c->cx1), y1 = ui_min(gy0 + g->h, c->cy1);
    int32_t px, py;

    for (py = y0; py < y1; py++) {
        const uint8_t *src = ui_font_alpha + g->off + (uint32_t)(py - gy0) * g->w;
        uint32_t *row = c->px + (int64_t)py * c->stride;

        for (px = x0; px < x1; px++) {
            uint32_t al = src[px - gx0];
            if (al == 0u)
                continue;
            al = (al * a + 127u) / 255u;
            row[px] = (al >= 255u) ? rgb : ui_mix(row[px], rgb, al);
        }
    }
}

int32_t ui_text_n(ui_canvas_t *c, int32_t x, int32_t y, const char *s, int32_t nbytes,
                  int font, uint32_t rgb, uint32_t a)
{
    const ui_glyph_t *tab = ui_fonts[font].g;
    int32_t i = 0;

    if (!s)
        return x;
    while (s[i] && i < nbytes) {
        uint32_t cp = ui_utf8_next(s, &i);
        const ui_glyph_t *g;

        if (cp == '\n' || cp == '\r')
            continue;
        g = &tab[glyph_index(cp)];
        if (c && g->w > 0)
            draw_glyph(c, x, y, g, rgb, a);
        x += g->adv;
    }
    return x;
}

int32_t ui_text(ui_canvas_t *c, int32_t x, int32_t y, const char *s, int font,
                uint32_t rgb, uint32_t a)
{
    return ui_text_n(c, x, y, s, 0x7FFFFFFF, font, rgb, a);
}

int32_t ui_text_width_n(const char *s, int32_t nbytes, int font)
{
    return ui_text_n(NULL, 0, 0, s, nbytes, font, 0, 0);
}

int32_t ui_text_width(const char *s, int font)
{
    return ui_text_n(NULL, 0, 0, s, 0x7FFFFFFF, font, 0, 0);
}

void ui_text_center(ui_canvas_t *c, int32_t cx, int32_t y, const char *s, int font,
                    uint32_t rgb, uint32_t a)
{
    ui_text(c, cx - ui_text_width(s, font) / 2, y, s, font, rgb, a);
}

void ui_text_fit(ui_canvas_t *c, int32_t x, int32_t y, const char *s, int font,
                 uint32_t rgb, uint32_t a, int32_t max_w)
{
    int32_t n, i, ell;

    if (ui_text_width(s, font) <= max_w) {
        ui_text(c, x, y, s, font, rgb, a);
        return;
    }
    ell = ui_text_width("…", font);
    n = (int32_t)strlen(s);
    i = n;
    while (i > 0 && ui_text_width_n(s, i, font) + ell > max_w)
        i = ui_utf8_prev(s, i);
    x = ui_text_n(c, x, y, s, i, font, rgb, a);
    ui_text(c, x, y, "…", font, rgb, a);
}

/* ---------------- cadenas ---------------- */

void ui_strlcpy(char *dst, const char *src, size_t cap)
{
    size_t i = 0;

    if (cap == 0)
        return;
    while (src && src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void ui_strlcat(char *dst, const char *src, size_t cap)
{
    size_t n = strlen(dst);

    if (n < cap)
        ui_strlcpy(dst + n, src, cap - n);
}

int ui_streq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

int ui_str_starts(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

static char ui_lower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
}

int ui_str_icontains(const char *hay, const char *needle)
{
    size_t i, j;

    if (!needle[0])
        return 1;
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j] && hay[i + j] && ui_lower(hay[i + j]) == ui_lower(needle[j]); j++)
            ;
        if (!needle[j])
            return 1;
    }
    return 0;
}

void ui_itoa(int64_t v, char *out)
{
    char tmp[24];
    int n = 0, neg = 0, i = 0;
    uint64_t u;

    if (v < 0) {
        neg = 1;
        u = (uint64_t)(-v);
    } else {
        u = (uint64_t)v;
    }
    do {
        tmp[n++] = (char)('0' + (int)(u % 10u));
        u /= 10u;
    } while (u && n < 22);
    if (neg)
        out[i++] = '-';
    while (n)
        out[i++] = tmp[--n];
    out[i] = '\0';
}

void ui_fmt2(char *out, int v)
{
    out[0] = (char)('0' + (v / 10) % 10);
    out[1] = (char)('0' + v % 10);
    out[2] = '\0';
}

/* ---------------- RTC CMOS ---------------- */

static uint8_t cmos(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static int bcd(uint8_t v, int is_bcd)
{
    return is_bcd ? (v & 0x0F) + ((v >> 4) * 10) : v;
}

void ui_rtc_read(ui_time_t *t)
{
    uint8_t regb, hr;
    int is_bcd, y, m, d;
    static const int k[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

    while (cmos(0x0A) & 0x80u)
        ;
    regb = cmos(0x0B);
    is_bcd = !(regb & 0x04u);
    t->second = bcd(cmos(0x00), is_bcd);
    t->minute = bcd(cmos(0x02), is_bcd);
    hr = cmos(0x04);
    t->hour = bcd((uint8_t)(hr & 0x7Fu), is_bcd);
    if (!(regb & 0x02u) && (hr & 0x80u))
        t->hour = (t->hour % 12) + 12;
    t->day = bcd(cmos(0x07), is_bcd);
    t->month = bcd(cmos(0x08), is_bcd);
    t->year = 2000 + bcd(cmos(0x09), is_bcd);

    y = t->year;
    m = t->month;
    d = t->day;
    if (m < 1 || m > 12)
        m = 1;
    if (m < 3)
        y -= 1;
    t->wday = (y + y / 4 - y / 100 + y / 400 + k[m - 1] + d) % 7;
}

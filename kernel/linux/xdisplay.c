/*
 * Servidor X11 de un solo cliente. Lo justo para que GTK/Cairo, sin RENDER,
 * cree la ventana de Firefox y vuelque el contenido con PutImage y CopyArea.
 */
#include <linux/xdisplay.h>

#include <interrupt/pit.h>
#include <log/klog.h>

#include <string.h>

#define XW LINUX_XDISPLAY_W
#define XH LINUX_XDISPLAY_H
#define WIN_N 64
#define PIX_N 2
#define ATOM_N 96
#define Q_N 65536u

struct xwin {
    uint32_t id;
    uint32_t parent;
    int16_t  x, y, w, h;
    int      used;
    int      mapped;
};

struct xpix {
    uint32_t  id;
    int16_t   w, h;
    int       used;
    uint32_t *px;
};

static uint32_t     g_screen[XW * XH];
static uint32_t     g_pool[PIX_N][XW * XH];
static struct xwin  g_win[WIN_N];
static struct xpix  g_pix[PIX_N];
static char         g_atom[ATOM_N][48];
static uint32_t     g_natoms;

static uint8_t  g_acc[4096];
static uint32_t g_acc_n;
static int      g_phase;
static uint32_t g_skip;
static uint16_t g_seq;
static int      g_fd = -1;
static int      g_logn;

static uint8_t  g_q[Q_N];
static uint32_t g_qr, g_qw, g_qn;

static int      g_stream;
static uint32_t g_stream_left;
static uint32_t g_pix_i, g_pix_max;
static int      g_pw, g_ph, g_pdx, g_pdy, g_pox, g_poy, g_pstride, g_dw, g_dh;
static uint32_t *g_pbase;
static int      g_pwin;
static uint8_t  g_carry[4];
static int      g_carry_n;
static uint64_t g_last_paint;
static linux_xdisplay_paint_fn g_paint;

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t pad4(uint32_t n)
{
    return (n + 3u) & ~3u;
}

static void q_put(const void *src, uint32_t n)
{
    const uint8_t *b = src;
    uint32_t i;

    for (i = 0; i < n; i++) {
        if (g_qn >= Q_N) {
            return;
        }
        g_q[g_qw] = b[i];
        g_qw = (g_qw + 1u) % Q_N;
        g_qn++;
    }
}

static void reply32(uint8_t data, const uint8_t tail[24])
{
    uint8_t h[32];

    memset(h, 0, sizeof(h));
    h[0] = 1;
    h[1] = data;
    wr16(h + 2, g_seq);
    q_put(h, 8);
    q_put(tail, 24);
}

static void event32(uint8_t code, const uint8_t body[28])
{
    uint8_t h[32];

    memset(h, 0, sizeof(h));
    h[0] = code;
    wr16(h + 2, g_seq);
    if (body != NULL) {
        memcpy(h + 4, body, 28);
    }
    q_put(h, 32);
}

static void paint(int force)
{
    uint64_t now;

    if (g_paint == NULL) {
        return;
    }
    now = pit_ticks_get();
    if (!force && g_last_paint != 0u && now - g_last_paint < 8u) {
        return;
    }
    g_last_paint = now;
    g_paint(g_screen, XW, XH);
}

static struct xwin *win_find(uint32_t id)
{
    int i;

    for (i = 0; i < WIN_N; i++) {
        if (g_win[i].used && g_win[i].id == id) {
            return &g_win[i];
        }
    }
    return NULL;
}

static struct xpix *pix_find(uint32_t id)
{
    int i;

    for (i = 0; i < PIX_N; i++) {
        if (g_pix[i].used && g_pix[i].id == id) {
            return &g_pix[i];
        }
    }
    return NULL;
}

static void origin(uint32_t id, int *x, int *y)
{
    int guard = 0;

    *x = 0;
    *y = 0;
    while (id != 0u && id != 0x100u && guard < 16) {
        struct xwin *w = win_find(id);

        if (w == NULL) {
            break;
        }
        *x += w->x;
        *y += w->y;
        id = w->parent;
        guard++;
    }
}

static int bind_drawable(uint32_t id, int *ox, int *oy, int *dw, int *dh,
                         uint32_t **base, int *stride, int *is_win)
{
    struct xwin *w = win_find(id);
    struct xpix *p;

    if (w != NULL) {
        origin(id, ox, oy);
        *dw = w->w;
        *dh = w->h;
        *base = g_screen;
        *stride = XW;
        *is_win = 1;
        return 1;
    }
    p = pix_find(id);
    if (p == NULL) {
        return 0;
    }
    *ox = 0;
    *oy = 0;
    *dw = p->w;
    *dh = p->h;
    *base = p->px;
    *stride = p->w > 0 ? p->w : 1;
    *is_win = 0;
    return 1;
}

static void put_px(int ox, int oy, int dw, int dh, uint32_t *base, int stride,
                   int is_win, int dx, int dy, uint32_t px, uint32_t col, uint32_t row)
{
    int x = dx + (int)col;
    int y = dy + (int)row;

    if (x < 0 || y < 0 || x >= dw || y >= dh) {
        return;
    }
    if (is_win) {
        x += ox;
        y += oy;
        if (x < 0 || y < 0 || x >= XW || y >= XH) {
            return;
        }
    }
    base[(uint32_t)y * (uint32_t)stride + (uint32_t)x] = px;
}

static void fill_rect(uint32_t id, int x, int y, int w, int h, uint32_t color)
{
    int ox, oy, dw, dh, stride, is_win, row, col;
    uint32_t *base;

    if (!bind_drawable(id, &ox, &oy, &dw, &dh, &base, &stride, &is_win)) {
        return;
    }
    if (w <= 0 || h <= 0) {
        x = 0;
        y = 0;
        w = dw;
        h = dh;
    }
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            put_px(ox, oy, dw, dh, base, stride, is_win, x, y, color,
                   (uint32_t)col, (uint32_t)row);
        }
    }
    if (is_win) {
        paint(0);
    }
}

static void copy_area(uint32_t src, uint32_t dst, int sx, int sy, int dx, int dy, int w, int h)
{
    int sox, soy, sdw, sdh, sstride, swin;
    int dox, doy, ddw, ddh, dstride, dwin;
    uint32_t *sb;
    uint32_t *db;
    int row, col;

    if (!bind_drawable(src, &sox, &soy, &sdw, &sdh, &sb, &sstride, &swin) ||
        !bind_drawable(dst, &dox, &doy, &ddw, &ddh, &db, &dstride, &dwin)) {
        return;
    }
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            int fx = sx + col;
            int fy = sy + row;
            uint32_t px = 0;

            if (swin) {
                fx += sox;
                fy += soy;
                if (fx >= 0 && fy >= 0 && fx < XW && fy < XH) {
                    px = g_screen[fy * XW + fx];
                }
            } else if (fx >= 0 && fy >= 0 && fx < sdw && fy < sdh) {
                px = sb[fy * sstride + fx];
            }
            put_px(dox, doy, ddw, ddh, db, dstride, dwin, dx, dy, px,
                   (uint32_t)col, (uint32_t)row);
        }
    }
    if (dwin) {
        paint(1);
    }
}

static uint32_t intern(const uint8_t *name, uint32_t len, int only)
{
    char tmp[48];
    uint32_t i;
    uint32_t n = len < 47u ? len : 47u;

    memcpy(tmp, name, n);
    tmp[n] = '\0';
    for (i = 0; i < g_natoms; i++) {
        if (strcmp(g_atom[i], tmp) == 0) {
            return i + 1u;
        }
    }
    if (only || g_natoms >= ATOM_N) {
        return 0;
    }
    memcpy(g_atom[g_natoms], tmp, n + 1u);
    g_natoms++;
    return g_natoms;
}

static void map_notify(uint32_t id)
{
    uint8_t b[28];
    struct xwin *w = win_find(id);
    int ox, oy;

    if (w == NULL) {
        return;
    }
    w->mapped = 1;
    origin(id, &ox, &oy);
    memset(b, 0, sizeof(b));
    wr32(b + 0, id);
    wr32(b + 4, id);
    wr16(b + 8, (uint16_t)ox);
    wr16(b + 10, (uint16_t)oy);
    wr16(b + 12, (uint16_t)w->w);
    wr16(b + 14, (uint16_t)w->h);
    event32(22, b);
    memset(b, 0, sizeof(b));
    wr32(b + 0, id);
    wr32(b + 4, id);
    event32(19, b);
    memset(b, 0, sizeof(b));
    wr32(b + 0, id);
    wr16(b + 4, 0);
    wr16(b + 6, 0);
    wr16(b + 8, (uint16_t)w->w);
    wr16(b + 10, (uint16_t)w->h);
    event32(12, b);
    paint(1);
}

static void send_setup(void)
{
    uint8_t b[160];
    uint32_t n = 8;

    memset(b, 0, sizeof(b));
    b[0] = 1;
    wr16(b + 2, 11);
    wr32(b + n, 1);
    n += 4;
    wr32(b + n, 0x200000u);
    n += 4;
    wr32(b + n, 0x1fffffu);
    n += 4;
    wr32(b + n, 256);
    n += 4;
    wr16(b + n, 7);
    n += 2;
    wr16(b + n, 65535u);
    n += 2;
    b[n++] = 1;
    b[n++] = 1;
    b[n++] = 0;
    b[n++] = 0;
    b[n++] = 32;
    b[n++] = 32;
    b[n++] = 8;
    b[n++] = 255;
    n += 4;
    memcpy(b + n, "MettaOS", 7);
    n += 8;
    b[n++] = 24;
    b[n++] = 32;
    b[n++] = 32;
    n += 5;
    wr32(b + n, 0x100u);
    n += 4;
    wr32(b + n, 0x101u);
    n += 4;
    wr32(b + n, 0x00ffffffu);
    n += 4;
    wr32(b + n, 0);
    n += 4;
    wr32(b + n, 0);
    n += 4;
    wr16(b + n, XW);
    n += 2;
    wr16(b + n, XH);
    n += 2;
    wr16(b + n, 211);
    n += 2;
    wr16(b + n, 127);
    n += 2;
    wr16(b + n, 1);
    n += 2;
    wr16(b + n, 1);
    n += 2;
    wr32(b + n, 0x102u);
    n += 4;
    b[n++] = 0;
    b[n++] = 0;
    b[n++] = 24;
    b[n++] = 1;
    b[n++] = 24;
    b[n++] = 0;
    wr16(b + n, 1);
    n += 2;
    n += 4;
    wr32(b + n, 0x102u);
    n += 4;
    b[n++] = 4;
    b[n++] = 8;
    wr16(b + n, 256);
    n += 2;
    wr32(b + n, 0x00ff0000u);
    n += 4;
    wr32(b + n, 0x0000ff00u);
    n += 4;
    wr32(b + n, 0x000000ffu);
    n += 4;
    n += 4;
    wr16(b + 6, (uint16_t)((n - 8u) / 4u));
    q_put(b, n);
    KLOG_INFO("x11 conectado %ux%u bytes %u\n", XW, XH, n);
}

static void begin_put(const uint8_t *r, uint32_t data_bytes)
{
    uint32_t id = rd32(r + 4);
    int ox, oy, dw, dh, stride, is_win;
    uint32_t *base;

    g_pbase = NULL;
    g_pix_i = 0;
    g_carry_n = 0;
    g_pw = (int)(int16_t)rd16(r + 12);
    g_ph = (int)(int16_t)rd16(r + 14);
    g_pdx = (int)(int16_t)rd16(r + 16);
    g_pdy = (int)(int16_t)rd16(r + 18);
    if (g_pw < 0) {
        g_pw = 0;
    }
    if (g_ph < 0) {
        g_ph = 0;
    }
    g_pix_max = (uint32_t)g_pw * (uint32_t)g_ph * 4u;
    g_pwin = 0;
    g_pbase = NULL;
    if (r[1] == 2 && bind_drawable(id, &ox, &oy, &dw, &dh, &base, &stride, &is_win)) {
        g_pox = ox;
        g_poy = oy;
        g_dw = dw;
        g_dh = dh;
        g_pbase = base;
        g_pstride = stride;
        g_pwin = is_win;
        /* dw/dh reused as clip via put_px; stash in pw/ph only for the source. */
        (void)dw;
        (void)dh;
    }
    (void)data_bytes;
}

static void emit_px(const uint8_t raw[4])
{
    uint32_t px;
    uint32_t col;
    uint32_t row;
    int ox, oy, dw, dh;

    if (g_pbase == NULL || g_pix_i >= g_pix_max) {
        g_pix_i += 4u;
        return;
    }
    memcpy(&px, raw, 4);
    col = (g_pix_i / 4u) % (uint32_t)(g_pw > 0 ? g_pw : 1);
    row = (g_pix_i / 4u) / (uint32_t)(g_pw > 0 ? g_pw : 1);
    ox = g_pwin ? g_pox : 0;
    oy = g_pwin ? g_poy : 0;
    dw = g_dw > 0 ? g_dw : 1;
    dh = g_dh > 0 ? g_dh : 1;
    put_px(ox, oy, dw, dh, g_pbase, g_pstride, g_pwin, g_pdx, g_pdy, px, col, row);
    g_pix_i += 4u;
}

static void eat_pixels(const uint8_t *p, uint32_t n)
{
    while (n > 0u) {
        g_carry[g_carry_n++] = *p++;
        n--;
        if (g_carry_n == 4) {
            emit_px(g_carry);
            g_carry_n = 0;
        }
    }
}

static void handle(const uint8_t *r, uint32_t size)
{
    uint8_t op = r[0];
    uint8_t tail[24];
    const uint8_t *body = r + 4;

    g_seq++;
    if (g_logn < 48) {
        g_logn++;
        KLOG_INFO("x11 req %u\n", (unsigned)op);
    }
    memset(tail, 0, sizeof(tail));
    switch (op) {
    case 1: {
        struct xwin *w = NULL;
        int i;
        uint32_t id = rd32(body);

        for (i = 0; i < WIN_N; i++) {
            if (!g_win[i].used || g_win[i].id == id) {
                w = &g_win[i];
                break;
            }
        }
        if (w != NULL && size >= 28u) {
            w->used = 1;
            w->id = id;
            w->parent = rd32(body + 4);
            w->x = (int16_t)rd16(body + 8);
            w->y = (int16_t)rd16(body + 10);
            w->w = (int16_t)rd16(body + 12);
            w->h = (int16_t)rd16(body + 14);
            w->mapped = 0;
            if (w->w <= 0) {
                w->w = XW;
            }
            if (w->h <= 0) {
                w->h = XH;
            }
        }
        break;
    }
    case 3: {
        struct xwin *w = win_find(rd32(body));
        uint8_t h[48];

        memset(h, 0, sizeof(h));
        h[0] = 1;
        wr16(h + 2, g_seq);
        wr32(h + 4, 3);
        wr32(h + 8, 0x102u);
        wr16(h + 12, 1);
        h[15] = 1;
        wr32(h + 16, 0xffffffffu);
        h[25] = 1;
        h[26] = (uint8_t)((w != NULL && w->mapped) ? 2 : 0);
        wr32(h + 28, 0x101u);
        q_put(h, 44);
        break;
    }
    case 8:
        map_notify(rd32(body));
        break;
    case 10: {
        struct xwin *w = win_find(rd32(body));
        uint8_t b[28];

        if (w != NULL) {
            w->mapped = 0;
        }
        memset(b, 0, sizeof(b));
        wr32(b, rd32(body));
        wr32(b + 4, rd32(body));
        event32(18, b);
        break;
    }
    case 12:
        if (size >= 12u) {
            struct xwin *w = win_find(rd32(body));
            uint16_t mask = rd16(body + 4);

            if (w != NULL) {
                const uint8_t *v = body + 8;

                if (mask & 1u) {
                    w->x = (int16_t)rd16(v);
                    v += 4;
                }
                if (mask & 2u) {
                    w->y = (int16_t)rd16(v);
                    v += 4;
                }
                if (mask & 4u) {
                    w->w = (int16_t)rd16(v);
                    v += 4;
                }
                if (mask & 8u) {
                    w->h = (int16_t)rd16(v);
                }
                if (w->mapped) {
                    map_notify(w->id);
                }
            }
        }
        break;
    case 14: {
        struct xwin *w = win_find(rd32(body));
        int ox = 0;
        int oy = 0;

        if (w != NULL) {
            origin(w->id, &ox, &oy);
            wr32(tail + 0, 0x100u);
            wr16(tail + 4, (uint16_t)ox);
            wr16(tail + 6, (uint16_t)oy);
            wr16(tail + 8, (uint16_t)w->w);
            wr16(tail + 10, (uint16_t)w->h);
        }
        reply32(24, tail);
        break;
    }
    case 15:
        wr32(tail + 0, 0x100u);
        reply32(0, tail);
        break;
    case 16: {
        uint16_t len = size >= 8u ? rd16(body) : 0;
        uint32_t id = 0;

        if (8u + len <= size) {
            id = intern(body + 4, len, r[1] != 0);
        }
        wr32(tail, id);
        reply32(0, tail);
        break;
    }
    case 17: {
        uint32_t id = rd32(body);
        uint8_t extra[64];
        uint32_t len = 0;

        memset(extra, 0, sizeof(extra));
        if (id >= 1u && id <= g_natoms) {
            len = (uint32_t)strlen(g_atom[id - 1u]);
            if (len > 47u) {
                len = 47u;
            }
            memcpy(extra, g_atom[id - 1u], len);
        }
        wr16(tail, (uint16_t)len);
        {
            uint8_t h[32];
            uint32_t extra_n = pad4(len);

            memset(h, 0, sizeof(h));
            h[0] = 1;
            wr16(h + 2, g_seq);
            wr32(h + 4, extra_n / 4u);
            wr16(h + 8, (uint16_t)len);
            q_put(h, 32);
            q_put(extra, extra_n);
        }
        break;
    }
    case 20:
        reply32(0, tail);
        break;
    case 23:
        reply32(0, tail);
        break;
    case 26:
    case 31:
        reply32(0, tail);
        break;
    case 38:
        wr32(tail + 0, 0x100u);
        tail[12] = 1;
        reply32(1, tail);
        break;
    case 40: {
        uint8_t h[32];

        memset(h, 0, sizeof(h));
        h[0] = 1;
        wr16(h + 2, g_seq);
        q_put(h, 32);
        break;
    }
    case 43:
        wr32(tail, 0x100u);
        reply32(2, tail);
        break;
    case 53: {
        int i;

        for (i = 0; i < PIX_N; i++) {
            if (!g_pix[i].used) {
                g_pix[i].used = 1;
                g_pix[i].id = rd32(body + 0);
                g_pix[i].w = (int16_t)rd16(body + 8);
                g_pix[i].h = (int16_t)rd16(body + 10);
                if (g_pix[i].w <= 0 || g_pix[i].w > XW) {
                    g_pix[i].w = XW;
                }
                if (g_pix[i].h <= 0 || g_pix[i].h > XH) {
                    g_pix[i].h = XH;
                }
                g_pix[i].px = g_pool[i];
                memset(g_pix[i].px, 0xff, (size_t)g_pix[i].w * (size_t)g_pix[i].h * 4u);
                break;
            }
        }
        break;
    }
    case 54: {
        struct xpix *p = pix_find(rd32(body));

        if (p != NULL) {
            p->used = 0;
        }
        break;
    }
    case 61:
        if (size >= 12u) {
            fill_rect(rd32(body), (int16_t)rd16(body + 4), (int16_t)rd16(body + 6),
                      (int16_t)rd16(body + 8), (int16_t)rd16(body + 10), 0x00ffffffu);
        }
        break;
    case 62:
        if (size >= 28u) {
            copy_area(rd32(body), rd32(body + 4),
                      (int16_t)rd16(body + 12), (int16_t)rd16(body + 14),
                      (int16_t)rd16(body + 16), (int16_t)rd16(body + 18),
                      (int16_t)rd16(body + 20), (int16_t)rd16(body + 22));
        }
        break;
    case 70: {
        uint32_t id = rd32(body);
        uint32_t off = 8;

        while (off + 8u <= size - 4u) {
            fill_rect(id, (int16_t)rd16(body + off), (int16_t)rd16(body + off + 2),
                      (int16_t)rd16(body + off + 4), (int16_t)rd16(body + off + 6),
                      0x00f2f2f2u);
            off += 8u;
        }
        break;
    }
    case 72:
        if (size >= 24u && r[1] == 2) {
            begin_put(r, size - 24u);
            if (size > 24u) {
                eat_pixels(r + 24, size - 24u);
            }
            if (g_pwin) {
                paint(1);
            }
        }
        break;
    case 84:
    case 85:
    case 92:
        wr16(tail + 0, 0xffffu);
        wr16(tail + 2, 0xffffu);
        wr16(tail + 4, 0xffffu);
        wr32(tail + 8, 0x00ffffffu);
        reply32(0, tail);
        break;
    case 98:
        reply32(0, tail);
        break;
    case 99:
        reply32(0, tail);
        break;
    case 101: {
        uint8_t count = size >= 6u ? body[1] : 0;
        uint8_t h[32];
        uint32_t i;

        memset(h, 0, sizeof(h));
        h[0] = 1;
        h[1] = 1;
        wr16(h + 2, g_seq);
        wr32(h + 4, count);
        q_put(h, 32);
        for (i = 0; i < count; i++) {
            uint32_t z = 0;
            q_put(&z, 4);
        }
        break;
    }
    case 108:
        reply32(0, tail);
        break;
    case 119: {
        uint8_t h[32];
        uint8_t z[8];

        memset(h, 0, sizeof(h));
        memset(z, 0, sizeof(z));
        h[0] = 1;
        h[1] = 1;
        wr16(h + 2, g_seq);
        wr32(h + 4, 2);
        q_put(h, 32);
        q_put(z, 8);
        break;
    }
    default:
        break;
    }
}

static void acc_consume(uint32_t n)
{
    if (n >= g_acc_n) {
        g_acc_n = 0;
        return;
    }
    {
        uint32_t i;

        for (i = 0; i < g_acc_n - n; i++) {
            g_acc[i] = g_acc[n + i];
        }
    }
    g_acc_n -= n;
}

static void parse(void)
{
    for (;;) {
        if (g_phase == 0) {
            uint16_t name_n;
            uint16_t data_n;

            if (g_acc_n < 12u) {
                return;
            }
            name_n = rd16(g_acc + 6);
            data_n = rd16(g_acc + 8);
            g_skip = pad4(name_n) + pad4(data_n);
            g_phase = 1;
            acc_consume(12);
        }
        if (g_phase == 1) {
            if (g_acc_n < g_skip) {
                return;
            }
            acc_consume(g_skip);
            send_setup();
            g_phase = 2;
        }
        if (g_phase == 2) {
            uint32_t total;

            if (g_acc_n < 4u) {
                return;
            }
            total = (uint32_t)rd16(g_acc + 2) * 4u;
            if (total < 4u) {
                acc_consume(4);
                continue;
            }
            if (total > sizeof(g_acc)) {
                if (g_acc[0] == 72 && g_acc_n < 24u) {
                    return;
                }
                if (g_acc[0] == 72 && g_acc_n >= 24u) {
                    g_seq++;
                    begin_put(g_acc, total - 24u);
                    g_stream = 1;
                    g_stream_left = total - 24u;
                    acc_consume(24);
                    return;
                }
                g_seq++;
                g_stream = 1;
                g_stream_left = total - 4u;
                g_pbase = NULL;
                acc_consume(4);
                return;
            }
            if (g_acc_n < total) {
                return;
            }
            handle(g_acc, total);
            acc_consume(total);
            continue;
        }
        return;
    }
}

void linux_xdisplay_on_paint(linux_xdisplay_paint_fn fn)
{
    g_paint = fn;
}

void linux_xdisplay_reset(void)
{
    memset(g_screen, 0xff, sizeof(g_screen));
    memset(g_win, 0, sizeof(g_win));
    memset(g_pix, 0, sizeof(g_pix));
    g_natoms = 0;
    g_acc_n = 0;
    g_phase = 0;
    g_skip = 0;
    g_seq = 0;
    g_fd = -1;
    g_logn = 0;
    g_qr = g_qw = g_qn = 0;
    g_stream = 0;
    g_stream_left = 0;
    g_carry_n = 0;
    g_last_paint = 0;
    g_win[0].used = 1;
    g_win[0].id = 0x100u;
    g_win[0].w = XW;
    g_win[0].h = XH;
    g_win[0].mapped = 1;
}

int linux_xdisplay_is_path(const void *addr, uint64_t len)
{
    const uint8_t *b = addr;
    const char needle[] = "X11-unix/X0";
    uint64_t i;
    uint64_t n;

    if (addr == NULL || len < 12u) {
        return 0;
    }
    n = len > 108u ? 108u : len;
    for (i = 0; i + sizeof(needle) - 1u <= n; i++) {
        uint64_t k;

        for (k = 0; needle[k] != '\0' && b[i + k] == (uint8_t)needle[k]; k++) {
        }
        if (needle[k] == '\0') {
            return 1;
        }
    }
    return 0;
}

void linux_xdisplay_attach(int fd)
{
    g_fd = fd;
    g_phase = 0;
    g_acc_n = 0;
    g_stream = 0;
    g_stream_left = 0;
    g_qr = g_qw = g_qn = 0;
}

int linux_xdisplay_owned(int fd)
{
    return fd >= 0 && fd == g_fd;
}

int linux_xdisplay_poll_in(int fd)
{
    return linux_xdisplay_owned(fd) && g_qn > 0u;
}

int64_t linux_xdisplay_read(int fd, void *buf, uint64_t count)
{
    uint8_t *out = buf;
    uint64_t n = 0;

    if (!linux_xdisplay_owned(fd)) {
        return -9;
    }
    if (g_qn == 0u) {
        return -11;
    }
    while (n < count && g_qn > 0u) {
        out[n++] = g_q[g_qr];
        g_qr = (g_qr + 1u) % Q_N;
        g_qn--;
    }
    return (int64_t)n;
}

int64_t linux_xdisplay_write(int fd, const void *buf, uint64_t count)
{
    const uint8_t *p = buf;
    uint64_t left = count;

    if (!linux_xdisplay_owned(fd)) {
        return -9;
    }
    while (left > 0u) {
        if (g_stream && g_stream_left > 0u) {
            uint32_t take = g_stream_left;

            if ((uint64_t)take > left) {
                take = (uint32_t)left;
            }
            if (g_pbase != NULL) {
                eat_pixels(p, take);
            }
            p += take;
            left -= take;
            g_stream_left -= take;
            if (g_stream_left == 0u) {
                g_stream = 0;
                if (g_pwin) {
                    paint(1);
                }
            }
            continue;
        }
        if (g_acc_n >= sizeof(g_acc)) {
            parse();
            if (g_acc_n >= sizeof(g_acc)) {
                break;
            }
        }
        {
            uint32_t room = (uint32_t)sizeof(g_acc) - g_acc_n;
            uint32_t take = room;

            if ((uint64_t)take > left) {
                take = (uint32_t)left;
            }
            memcpy(g_acc + g_acc_n, p, take);
            g_acc_n += take;
            p += take;
            left -= take;
            parse();
        }
    }
    return (int64_t)(count - left);
}

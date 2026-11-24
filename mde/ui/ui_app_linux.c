#include "ui_app.h"
#include "../compositor/compositor.h"
#include <linux/desktop_link.h>
#include <linux/usermode.h>
#include <linux/xdisplay.h>
#include <fs/squashfs_zlib.h>
#include <log/klog.h>
#include <metta/pmm.h>
#include <fs/vfs.h>

enum { WIDTH = 800, HEIGHT = 480, STRIP = 8, TOP = UI_TITLEBAR_H };
enum { WAITING, STARTING, RUNNING, FAILED };
typedef struct {
    int state, image, inflight, reset;
    uint64_t last_request;
    char message[112];
    int file, importing, pending, local;
    uint8_t chunk[4096];
    metta_phys_frame_t *pages[HEIGHT];
    uint32_t *rows[HEIGHT];
} surface_t;

static surface_t surfaces[UI_MAX_WIN];
static uint32_t strip[STRIP][WIDTH];
static int attached, online;
static uint64_t hello;
static char image_names[FS_IMAGE_LIMIT][256];

static void view(const ui_window_t *w, ui_rect_t *r)
{
    int h = w->h - TOP - 4, width = w->w - 8;
    if (width * HEIGHT > h * WIDTH) width = h * WIDTH / HEIGHT;
    else h = width * HEIGHT / WIDTH;
    *r = (ui_rect_t){(w->w - width) / 2, TOP + (w->h - TOP - h) / 2, width, h};
}

static void fail(ui_window_t *w, surface_t *s, const char *message)
{
    s->state = FAILED;
    ui_strlcpy(s->message, message, sizeof(s->message));
    ui_win_invalidate(w);
}

static void start(int slot)
{
    surface_t *s = &surfaces[slot];
    uint8_t id = (uint8_t)s->image;
    if (s->importing) {
        if (desktop_link_send('V', slot+1, NULL, 0)) s->state = STARTING;
        return;
    }
    if (desktop_link_send(s->image < 0 ? 'B' : 'A', slot + 1, &id, s->image < 0 ? 0 : 1))
        s->state = STARTING;
}

static void receive(char type, unsigned channel, const uint8_t *data, unsigned bytes)
{
    char name[256];
    if (type == 'H' && channel == 0 && bytes == 4 &&
        data[0] == 'M' && data[1] == 'D' && data[2] == 'E' && data[3] == '1') {
        if (!online) KLOG_INFO("MDE: Linux graphics link ready\n");
        online = 1;
        return;
    }
    if (type == 'I' && channel < FS_IMAGE_LIMIT && bytes && bytes < sizeof(name)) {
        memcpy(name, data, bytes);
        name[bytes] = 0;
        ui_strlcpy(image_names[channel], name, sizeof(image_names[channel]));
        fs_import_image(channel, name);
        return;
    }
    if (type == 'L') {
        ui_invalidate(0, 0, ui_screen_w(), ui_screen_h());
        return;
    }
    if (!channel || channel > UI_MAX_WIN) return;
    int slot = (int)channel - 1;
    ui_window_t *w = ui_window_by_slot(slot);
    if (!w || w->app != APP_LINUX) return;
    surface_t *s = &surfaces[slot];
    switch (type) {
    case 'O':
        s->state = RUNNING;
        s->reset = 1;
        s->inflight = 0;
        ui_win_invalidate(w);
        break;
    case 'E':
        s->inflight = 0;
        break;
    case 'X':
        ui_close_window(w);
        break;
    case 'S':
        bytes = ui_min((int)bytes, (int)sizeof(name)-1);
        memcpy(name, data, bytes);
        name[bytes] = 0;
        fail(w, s, name);
        break;
    case 'G': {
        if (s->state != RUNNING || bytes < 3) break;
        unsigned y = data[0] | ((unsigned)data[1] << 8);
        if (y % STRIP || y > HEIGHT - STRIP) break;
        uint64_t size = 0;
        if (squashfs_zlib_decompress(data + 2, bytes - 2, strip, sizeof(strip), &size) ||
            size != sizeof(strip)) { s->reset = 1; break; }
        for (unsigned i = 0; i < STRIP; i++) {
            if (!s->rows[y+i]) {
                if (pmm_frame_alloc(METTA_FRAME_KERNEL_HEAP, &s->pages[y+i]) != METTA_OK) {
                    desktop_link_send('C', channel, NULL, 0);
                    fail(w, s, "Memoria insuficiente para la ventana.");
                    return;
                }
                s->rows[y+i] = (void *)(uintptr_t)pmm_frame_phys(s->pages[y+i]);
            }
            memcpy(s->rows[y+i], strip[i], sizeof(strip[i]));
        }
        ui_rect_t r;
        view(w, &r);
        int first = (int)y * r.h / HEIGHT;
        int last = ((int)(y + STRIP) * r.h + HEIGHT - 1) / HEIGHT;
        if (!w->minimized) ui_win_invalidate_local(w, r.x, r.y + first, r.w, last - first);
        break;
    }
    }
}

void ui_linux_init(void) { attached = desktop_link_init(); }
int ui_linux_available(void) { return online; }
int ui_linux_attached(void) { return attached; }

void ui_linux_poll(uint64_t now)
{
    if (!attached) return;
    desktop_link_poll(receive);
    if (!online && (!hello || now - hello >= 2000)) {
        desktop_link_send('?', 0, NULL, 0);
        hello = now;
    }
}

static int path_has(const char *path, const char *needle)
{
    const char *p;
    const char *n;

    if (path == NULL || needle == NULL) {
        return 0;
    }
    for (p = path; *p != '\0'; p++) {
        for (n = needle; *n != '\0' && p[n - needle] == *n; n++) {
        }
        if (*n == '\0') {
            return 1;
        }
    }
    return 0;
}

static int g_ff_slot = -1;
static int g_ff_got;

static void firefox_paint(const uint32_t *px, int w, int h)
{
    surface_t *s;
    ui_window_t *win;
    int y;
    int copy_w;

    if (g_ff_slot < 0 || px == NULL)
        return;
    s = &surfaces[g_ff_slot];
    if (w > WIDTH)
        w = WIDTH;
    if (h > HEIGHT)
        h = HEIGHT;
    copy_w = w;
    for (y = 0; y < h; y++) {
        if (s->rows[y] != NULL)
            memcpy(s->rows[y], px + (size_t)y * LINUX_XDISPLAY_W, (size_t)copy_w * 4u);
    }
    s->state = RUNNING;
    g_ff_got = 1;
    win = ui_window_by_slot(g_ff_slot);
    if (win != NULL)
        ui_win_invalidate(win);
    compositor_flush_now();
}

static int firefox_rows(surface_t *s)
{
    int y;

    for (y = 0; y < HEIGHT; y++) {
        if (s->rows[y] == NULL) {
            if (pmm_frame_alloc(METTA_FRAME_KERNEL_HEAP, &s->pages[y]) != METTA_OK)
                return -1;
            s->rows[y] = (void *)(uintptr_t)pmm_frame_phys(s->pages[y]);
        }
        memset(s->rows[y], 0xff, WIDTH * 4u);
    }
    return 0;
}

static void linux_open(ui_window_t *w, int slot, const char *arg)
{
    surface_t *s = &surfaces[slot];
    s->image = -1;
    s->local = 0;
    ui_strlcpy(w->title, "GNU Bash", sizeof(w->title));
    if (path_has(arg, "Firefox.AppImage") || path_has(arg, "firefox-bin")) {
        int status;

        ui_strlcpy(w->title, "Firefox", sizeof(w->title));
        ui_strlcpy(s->message, "Cargando Firefox…", sizeof(s->message));
        s->local = 1;
        s->state = STARTING;
        if (firefox_rows(s) != 0) {
            fail(w, s, "Memoria insuficiente para la ventana.");
            return;
        }
        g_ff_slot = slot;
        g_ff_got = 0;
        linux_xdisplay_on_paint(firefox_paint);
        compositor_flush_now();
        status = linux_user_run_firefox();
        linux_xdisplay_on_paint(NULL);
        g_ff_slot = -1;
        KLOG_INFO("firefox: exit %d\n", status);
        if (!g_ff_got) {
            if (status == 0)
                fail(w, s, "Firefox terminó.");
            else
                fail(w, s, "Firefox arrancó y terminó. Todavía no hay pantalla.");
        }
        return;
    }
    if (arg && arg[0] == '/') {
        if (vfs_open(arg, VFS_O_RDONLY, &s->file) != METTA_OK) {
            fail(w,s,"El archivo no está disponible en el disco MFS.");
            return;
        }
        s->importing = 1;
        const char *name = arg;
        for (const char *c = arg; *c; c++) if (*c == '/') name=c+1;
        ui_strlcpy(w->title,name,sizeof(w->title));
    } else if (arg) {
        int id = fs_find(fs_apps_dir(), arg) - FS_MAX_NODES;
        if (id < 0 || id >= FS_IMAGE_LIMIT || !image_names[id][0]) {
            fail(w, s, "Este AppImage no está en el medio de aplicaciones.");
            return;
        }
        s->image = id;
        ui_strlcpy(w->title, image_names[id], sizeof(w->title));
    }
    if (!attached) {
        fail(w, s, "Este programa todavía no se ejecuta dentro de MettaOS.");
        return;
    }
    s->state = WAITING;
    ui_strlcpy(s->message, "Iniciando el entorno Linux…", sizeof(s->message));
    if (online) start(slot);
}

void ui_linux_close(int slot)
{
    surface_t *s = &surfaces[slot];
    if (online) desktop_link_send('C', slot + 1, NULL, 0);
    if (s->importing) vfs_close(s->file);
    for (int y = 0; y < HEIGHT; y++)
        if (s->pages[y]) pmm_frame_free(s->pages[y]);
    memset(s, 0, sizeof(*s));
}

void ui_linux_release(int slot)
{
    if (online) desktop_link_send('U', slot + 1, NULL, 0);
}

static void linux_draw(ui_window_t *w, int slot, ui_canvas_t *c, int32_t ox, int32_t oy, int focused)
{
    (void)focused;
    surface_t *s = &surfaces[slot];
    if (s->state != RUNNING) {
        ui_text_center(c, ox + w->w/2, oy + w->h/2, s->message, UI_FONT_UI, UI_TEXT, 255);
        return;
    }
    ui_rect_t r;
    view(w, &r);
    int left = ox + r.x, top = oy + r.y;
    int x0 = ui_max(0, c->cx0-left), x1 = ui_min(r.w, c->cx1-left);
    if (x0 >= x1) return;
    for (int y = ui_max(0, c->cy0-top); y < ui_min(r.h, c->cy1-top); y++) {
        uint32_t *src = s->rows[y * HEIGHT / r.h];
        if (!src) continue;
        uint32_t *dst = c->px + (y+top)*c->stride+left;
        if (r.w == WIDTH) memcpy(dst+x0, src+x0, (size_t)(x1-x0)*4);
        else for (int x=x0; x<x1; x++) dst[x] = src[x * WIDTH / r.w];
    }
}

static int linux_tick(ui_window_t *w, int slot, uint64_t now)
{
    surface_t *s = &surfaces[slot];
    if (s->local) return 0;
    if (!online) return 0;
    if (s->state == WAITING) start(slot);
    if (s->state == STARTING && s->importing) {
        for (int i=0; i<8; i++) {
            if (!s->pending)
                s->pending = (int)vfs_read(s->file, s->chunk, sizeof(s->chunk));
            if (s->pending < 0) {
                desktop_link_send('C',slot+1,NULL,0);
                fail(w,s,"Error al leer el AppImage del disco MFS.");
                break;
            }
            if (!s->pending) {
                if (desktop_link_send('Z',slot+1,NULL,0)) {
                    vfs_close(s->file);
                    s->importing = 0;
                }
                break;
            }
            if (!desktop_link_send('D',slot+1,s->chunk,(unsigned)s->pending)) break;
            s->pending = 0;
        }
    }
    if (s->state == RUNNING && !s->inflight &&
        now - s->last_request >= (ui_win_focused(w) ? 80u : 500u)) {
        uint8_t reset = (uint8_t)s->reset;
        if (desktop_link_send('F', slot+1, &reset, 1)) {
            s->reset = 0;
            s->inflight = 1;
            s->last_request = now;
        }
    }
    return 0;
}

void ui_linux_key(int slot, const input_event_t *ev)
{
    /* Set-1 codes match Linux evdev except for the extended cluster. */
    static const uint8_t extended[] = { 103,108,105,106,102,107,104,109,110,111,97,100 };
    unsigned key = ev->u.key.keycode, code = ev->u.key.scancode;
    if (key >= INPUT_KEY_UP && key <= INPUT_KEY_RIGHTALT) code = extended[key - INPUT_KEY_UP];
    if (surfaces[slot].local || !code || surfaces[slot].state != RUNNING) return;
    uint8_t msg[2] = {(uint8_t)(code+8), ev->type == INPUT_EV_KEY_DOWN};
    desktop_link_send('K', slot+1, msg, 2);
}

static void linux_mouse(ui_window_t *w, int slot, int type, int32_t x, int32_t y, int btn, int clicks)
{
    (void)clicks;
    if (surfaces[slot].local || surfaces[slot].state != RUNNING || type == UI_M_LEAVE) return;
    ui_rect_t r;
    view(w, &r);
    if (type != UI_M_UP && !ui_rect_has(&r, x, y)) return;
    uint16_t coord[2] = {(uint16_t)(ui_clampi(x-r.x,0,r.w-1)*WIDTH/r.w),
                         (uint16_t)(ui_clampi(y-r.y,0,r.h-1)*HEIGHT/r.h)};
    desktop_link_send('M', slot+1, coord, sizeof(coord));
    if (type == UI_M_DOWN || type == UI_M_UP) {
        uint8_t msg[2] = {btn==INPUT_BTN_LEFT ? 1 : btn==INPUT_BTN_RIGHT ? 3 : 2, type==UI_M_DOWN};
        desktop_link_send('P', slot+1, msg, 2);
        ui_consume();
    }
}

static void linux_wheel(ui_window_t *w, int slot, int delta)
{
    (void)w;
    if (surfaces[slot].local) return;
    uint8_t msg[2] = {delta > 0 ? 4 : 5, 1};
    desktop_link_send('P', slot+1, msg, 2);
    msg[1] = 0;
    desktop_link_send('P', slot+1, msg, 2);
}

const ui_app_t app_linux = {
    "GNU Bash", UI_ICON_TERMINAL, WIDTH+8, HEIGHT+TOP+4, 408, 288, 0xF4F7F6, 0, 0, 1, 0,
    linux_open, linux_draw, linux_mouse, NULL, linux_wheel, linux_tick, NULL, NULL,
};

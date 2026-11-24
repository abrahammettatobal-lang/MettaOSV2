/*
 * MettaOS shell — contrato ventana/app y servicios del shell para apps.
 */
#ifndef MDE_UI_APP_H
#define MDE_UI_APP_H

#include "ui_internal.h"

#define UI_MAX_WIN      12
#define UI_TITLEBAR_H   44

enum {
    APP_FILES = 0,
    APP_NOTES,
    APP_TERMINAL,
    APP_CALC,
    APP_SETTINGS,
    APP_LINUX,
    APP_COUNT
};

enum { UI_M_DOWN = 0, UI_M_UP, UI_M_DRAG, UI_M_HOVER, UI_M_LEAVE };

typedef struct ui_window {
    int      used;
    int      id;
    int      app;
    char     title[64];
    int32_t  x, y, w, h;
    int32_t  rx, ry, rw, rh;
    int      minimized;
    int      maximized;
} ui_window_t;

typedef struct {
    const char *name;
    int         icon;
    int32_t     def_w, def_h, min_w, min_h;
    uint32_t    bg;          /* fondo de ventana opaco */
    int         glass_body;  /* 1 = cuerpo Liquid Glass oscuro */
    int         dark;        /* título/semáforos sobre fondo oscuro */
    int         show_title;
    int         single;      /* una sola ventana: reabrir = enfocar */
    void (*open)(ui_window_t *w, int slot, const char *arg);
    void (*draw)(ui_window_t *w, int slot, ui_canvas_t *c, int32_t ox, int32_t oy, int focused);
    void (*mouse)(ui_window_t *w, int slot, int type, int32_t lx, int32_t ly, int btn, int clicks);
    void (*key)(ui_window_t *w, int slot, const input_event_t *ev, const char *text);
    void (*wheel)(ui_window_t *w, int slot, int delta);
    int  (*tick)(ui_window_t *w, int slot, uint64_t now);
    int  (*glass)(ui_window_t *w, int slot, ui_rect_t *out, int max); /* rects locales */
    int  (*new_cmd)(ui_window_t *w, int slot); /* Ctrl+N propio; 1 = consumido */
} ui_app_t;

extern const ui_app_t *const ui_apps[APP_COUNT];

/* servicios del shell */
void         ui_invalidate(int32_t x, int32_t y, int32_t w, int32_t h);
void         ui_win_invalidate(const ui_window_t *w);
void         ui_win_invalidate_local(const ui_window_t *w, int32_t x, int32_t y, int32_t ww, int32_t hh);
ui_window_t *ui_open_app(int app, const char *arg);
void         ui_close_window(ui_window_t *w);
int          ui_win_slot(const ui_window_t *w);
ui_window_t *ui_window_by_slot(int slot);
int          ui_win_focused(const ui_window_t *w);
void         ui_consume(void); /* la app manejó el clic: no arrastrar ventana */
uint64_t     ui_now(void);
int32_t      ui_screen_w(void);
int32_t      ui_screen_h(void);
int          ui_wallpaper_get(void);
int          ui_liquid_glass(void);
int          ui_set_liquid_glass(int enabled);
int          fs_read_glass(void);
int          fs_write_glass(int enabled);
void         ui_wallpaper_set(int idx);
int          ui_kbd_layout_get(void);   /* 0 = Latinoamérica, 1 = EE. UU. */
void         ui_kbd_layout_set(int idx);
void         ui_reboot(void);
void         ui_shutdown(void);

typedef struct {
    const char *label;
    int         id;
    int         disabled;
    const char *shortcut;
} ui_menu_item_t;

typedef void (*ui_menu_cb)(int id, void *ctx);
void ui_menu_popup(int32_t x, int32_t y, const ui_menu_item_t *items, int n, ui_menu_cb cb, void *ctx);

/* glass compartidos */
extern const ui_glass_t UI_GLASS_LIGHT;   /* dock, sidebars */
extern const ui_glass_t UI_GLASS_MENU;    /* menús, popovers */
extern const ui_glass_t UI_GLASS_DARK;    /* calculadora, terminal */

#define UI_ACCENT      0x186A70u
#define UI_TEXT        0x172C35u
#define UI_TEXT_2      0x516770u
#define UI_SEPARATOR   0xCDDDDAu

/* ---------- sistema de archivos en RAM ---------- */

enum { FS_DIR = 0, FS_TEXT, FS_APPIMAGE, FS_APP };

#define FS_MAX_NODES  160
#define FS_IMAGE_LIMIT 64
#define FS_NODE_CAP (FS_MAX_NODES + FS_IMAGE_LIMIT)
#define FS_FILE_CAP   8192

typedef struct {
    int  used;
    char name[48];
    int  parent;
    int  type;
    int  app;   /* FS_APP / FS_APPIMAGE: app a lanzar */
    int  slot;  /* FS_TEXT: buffer de datos */
    int  size;
} fs_node_t;

void fs_changed(int idx);
uint64_t fs_revision(void);
void fs_sync(void);
const char *fs_disk_status(void);
void       fs_init(void);
fs_node_t *fs_node(int idx);
int        fs_root(void);
int        fs_home(void);
int        fs_desktop(void);
int        fs_docs(void);
int        fs_trash(void);
int        fs_apps_dir(void);
int        fs_find(int parent, const char *name);
int        fs_lookup(int cwd, const char *path);
int        fs_create(int parent, const char *name, int type);
int        fs_remove(int idx);
int        fs_move(int idx, int new_parent);
int        fs_unique_name(int parent, const char *base, const char *ext, char *out, size_t cap);
void       fs_path(int idx, char *out, size_t cap);
char      *fs_data(int idx);
void       fs_set_text(int idx, const char *text);
int        fs_child_next(int parent, int after); /* -1 inicio; devuelve -1 al final */
int        fs_count_children(int parent);
int        fs_icon(int idx);
void       fs_open(int idx); /* abre según tipo */
void       fs_import_image(unsigned index, const char *name);
void       ui_linux_init(void);
void       ui_linux_poll(uint64_t now);
int        ui_linux_available(void);
int        ui_linux_attached(void);
void       ui_linux_close(int slot);
void       ui_linux_release(int slot);
void       ui_linux_key(int slot, const input_event_t *ev);

#endif /* MDE_UI_APP_H */

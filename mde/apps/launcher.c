/*
 * MettaOS MDE — launcher (.mettaapp) + overlay Liquid Glass (SA-29 / LG-06).
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "launcher.h"
#include "../theme/glass.h"

#define METTAAPP_MAGIC_LINE   "mettaapp/1"
#define LAUNCHER_MAX_ENTRIES  32u

void mde_anim_tick(uint64_t now_ms);
void mde_anim_launcher_enter(uint64_t now_ms);
void mde_anim_launcher_dismiss(uint64_t now_ms);
void mde_anim_overlay_reset_closed(uint32_t fade_ch, uint32_t scale_ch);
int32_t mde_anim_launcher_opacity(void);
int32_t mde_anim_launcher_scale(void);

#define MDE_ANIM_CH_LAUNCHER_FADE  62u
#define MDE_ANIM_CH_LAUNCHER_SCALE 63u

#define MDE_LAUNCHER_PANEL_W       480
#define MDE_LAUNCHER_PANEL_H       520
#define MDE_LAUNCHER_SEARCH_H      36
#define MDE_LAUNCHER_GRID_CELL     96
#define MDE_LAUNCHER_GRID_COLS     4
#define MDE_LAUNCHER_QUERY_MAX     64

typedef struct {
    mde_launcher_entry_t entries[LAUNCHER_MAX_ENTRIES];
    uint32_t             count;
    uint32_t             ready;
} mde_launcher_state_t;

typedef struct {
    int              open;
    int              anim_armed;
    int32_t          screen_w;
    int32_t          screen_h;
    char             query[MDE_LAUNCHER_QUERY_MAX];
    uint32_t         visible_map[LAUNCHER_MAX_ENTRIES];
    uint32_t         visible_count;
    int32_t          hover_index;
    mde_glass_surface_style panel_glass;
    mde_glass_surface_style search_glass;
    mde_launcher_ui_layout_t layout;
} mde_launcher_ui_t;

static mde_launcher_state_t g_launcher;
static mde_launcher_ui_t    g_launcher_ui;

static const char g_builtin_notes_manifest[] =
    "mettaapp/1\n"
    "id=notes\n"
    "name=Notes\n"
    "version=0.1.0\n"
    "entry=bin/notes.elf\n"
    "icon=icons/notes.png\n"
    "kind=gui\n";

static const char g_builtin_files_manifest[] =
    "mettaapp/1\n"
    "id=files\n"
    "name=Files\n"
    "version=0.1.0\n"
    "entry=bin/files.elf\n"
    "icon=icons/files.png\n"
    "kind=gui\n";

static mde_app_kind_t launcher_kind_from_string(const char *s)
{
    if (s == NULL) {
        return MDE_APP_KIND_UNKNOWN;
    }
    if (strcmp(s, "gui") == 0) {
        return MDE_APP_KIND_GUI;
    }
    if (strcmp(s, "cli") == 0) {
        return MDE_APP_KIND_CLI;
    }
    if (strcmp(s, "service") == 0) {
        return MDE_APP_KIND_SERVICE;
    }
    return MDE_APP_KIND_UNKNOWN;
}

static int launcher_copy_field(char *dst, size_t dst_sz, const char *src)
{
    size_t n;

    if (dst == NULL || src == NULL || dst_sz == 0u) {
        return -1;
    }
    n = strlen(src);
    if (n >= dst_sz) {
        return -1;
    }
    memcpy(dst, src, n + 1u);
    return 0;
}

static void launcher_entry_clear(mde_launcher_entry_t *e)
{
    if (e == NULL) {
        return;
    }
    memset(e, 0, sizeof(*e));
    e->kind = MDE_APP_KIND_UNKNOWN;
}

static const char *launcher_skip_line(const char *p)
{
    while (*p != '\0' && *p != '\n') {
        p++;
    }
    if (*p == '\n') {
        p++;
    }
    return p;
}

static int launcher_apply_kv(mde_launcher_entry_t *e, const char *key, const char *val)
{
    if (e == NULL || key == NULL || val == NULL) {
        return -1;
    }
    if (strcmp(key, "id") == 0) {
        return launcher_copy_field(e->id, sizeof(e->id), val);
    }
    if (strcmp(key, "name") == 0) {
        return launcher_copy_field(e->name, sizeof(e->name), val);
    }
    if (strcmp(key, "version") == 0) {
        return launcher_copy_field(e->version, sizeof(e->version), val);
    }
    if (strcmp(key, "entry") == 0) {
        return launcher_copy_field(e->entry, sizeof(e->entry), val);
    }
    if (strcmp(key, "icon") == 0) {
        return launcher_copy_field(e->icon, sizeof(e->icon), val);
    }
    if (strcmp(key, "kind") == 0) {
        mde_app_kind_t k = launcher_kind_from_string(val);
        if (k == MDE_APP_KIND_UNKNOWN) {
            return -1;
        }
        e->kind = k;
        return 0;
    }
    return 0;
}

int mde_launcher_parse_manifest(const char *text, mde_launcher_entry_t *out)
{
    const char *p;
    const char *end;
    int         saw_magic;
    size_t      text_len;

    if (text == NULL || out == NULL) {
        return -1;
    }
    text_len = strlen(text);
    if (text_len == 0u) {
        return -1;
    }

    launcher_entry_clear(out);
    p = text;
    end = text + text_len;
    saw_magic = 0;

    while (p < end && *p != '\0') {
        const char *line_start = p;
        const char *line_end;
        size_t      line_len;
        char        line_buf[512];
        char       *eq;

        p = launcher_skip_line(p);
        line_end = p;
        if (line_end > line_start && *(line_end - 1) == '\n') {
            line_end--;
        }
        line_len = (size_t)(line_end - line_start);
        if (line_len == 0u) {
            continue;
        }
        if (line_len >= sizeof(line_buf)) {
            return -1;
        }
        memcpy(line_buf, line_start, line_len);
        line_buf[line_len] = '\0';

        if (line_buf[0] == '#') {
            continue;
        }

        if (!saw_magic) {
            if (strcmp(line_buf, METTAAPP_MAGIC_LINE) != 0) {
                return -1;
            }
            saw_magic = 1;
            continue;
        }

        eq = strchr(line_buf, '=');
        if (eq == NULL) {
            return -1;
        }
        *eq = '\0';
        if (launcher_apply_kv(out, line_buf, eq + 1) != 0) {
            return -1;
        }
    }

    if (!saw_magic || out->id[0] == '\0' || out->name[0] == '\0' || out->version[0] == '\0'
        || out->entry[0] == '\0' || out->kind == MDE_APP_KIND_UNKNOWN) {
        return -1;
    }
    return 0;
}

static int launcher_add_entry(const mde_launcher_entry_t *entry)
{
    if (!g_launcher.ready || entry == NULL) {
        return -1;
    }
    if (g_launcher.count >= LAUNCHER_MAX_ENTRIES) {
        return -1;
    }
    g_launcher.entries[g_launcher.count] = *entry;
    g_launcher.count++;
    return 0;
}

static int launcher_query_matches(const char *name, const char *id, const char *q)
{
    size_t qi;
    size_t i;

    if (q == NULL || q[0] == '\0') {
        return 1;
    }
  qi = strlen(q);
  if (name != NULL) {
    for (i = 0; name[i] != '\0'; i++) {
      size_t j;
      for (j = 0; j < qi && name[i + j] != '\0'; j++) {
        char a = name[i + j];
        char b = q[j];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a + ('a' - 'A'));
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b + ('a' - 'A'));
        }
        if (a != b) {
            break;
        }
      }
      if (j == qi) {
          return 1;
      }
    }
  }
  if (id != NULL && strstr(id, q) != NULL) {
      return 1;
  }
  return 0;
}

static void launcher_ui_rebuild_filter(void)
{
    uint32_t i;
    uint32_t n = 0;

    g_launcher_ui.visible_count = 0;
    if (!g_launcher.ready) {
        return;
    }
    for (i = 0; i < g_launcher.count; i++) {
        const mde_launcher_entry_t *e = &g_launcher.entries[i];
        if (e->kind != MDE_APP_KIND_GUI) {
            continue;
        }
        if (!launcher_query_matches(e->name, e->id, g_launcher_ui.query)) {
            continue;
        }
        g_launcher_ui.visible_map[n] = i;
        n++;
    }
    g_launcher_ui.visible_count = n;
}

static void launcher_ui_relayout(void)
{
    int32_t pw = MDE_LAUNCHER_PANEL_W;
    int32_t ph = MDE_LAUNCHER_PANEL_H;
    int32_t cx;
    int32_t cy;
    uint32_t rows;

    cx = (g_launcher_ui.screen_w - pw) / 2;
    cy = (g_launcher_ui.screen_h - ph) / 2;
    if (cx < 0) {
        cx = 0;
    }
    if (cy < 0) {
        cy = 0;
    }

    rows = g_launcher_ui.visible_count;
    if (rows == 0) {
        rows = 1;
    }
    rows = (rows + MDE_LAUNCHER_GRID_COLS - 1u) / MDE_LAUNCHER_GRID_COLS;

    g_launcher_ui.layout.panel_x = cx;
    g_launcher_ui.layout.panel_y = cy;
    g_launcher_ui.layout.panel_w = pw;
    g_launcher_ui.layout.panel_h = ph;
    g_launcher_ui.layout.search_x = cx + 24;
    g_launcher_ui.layout.search_y = cy + 24;
    g_launcher_ui.layout.search_w = pw - 48;
    g_launcher_ui.layout.search_h = MDE_LAUNCHER_SEARCH_H;
    g_launcher_ui.layout.grid_x = cx + 24;
    g_launcher_ui.layout.grid_y = cy + 24 + MDE_LAUNCHER_SEARCH_H + 16;
    g_launcher_ui.layout.grid_cols = MDE_LAUNCHER_GRID_COLS;
    g_launcher_ui.layout.grid_cell = MDE_LAUNCHER_GRID_CELL;
    g_launcher_ui.layout.grid_rows = rows;
    g_launcher_ui.layout.visible_count = g_launcher_ui.visible_count;

    mde_glass_window_style_light(&g_launcher_ui.panel_glass);
    g_launcher_ui.panel_glass.corner_radius = MDE_CTRL_GLASS_MENU_RADIUS;

    mde_glass_surface_fill(&g_launcher_ui.search_glass, &mde_light_glass);
    g_launcher_ui.search_glass.fill          = MDE_GLASS_FILL_CHROME;
    g_launcher_ui.search_glass.corner_radius = MDE_CTRL_GLASS_MENU_RADIUS;
    g_launcher_ui.search_glass.blur_radius   = (uint8_t)MDE_CTRL_GLASS_MENU_BLUR;
}

int mde_launcher_init(void)
{
    mde_launcher_entry_t tmp;

    memset(&g_launcher, 0, sizeof(g_launcher));
    memset(&g_launcher_ui, 0, sizeof(g_launcher_ui));
    g_launcher.ready = 1u;

    if (mde_launcher_parse_manifest(g_builtin_notes_manifest, &tmp) != 0) {
        g_launcher.ready = 0u;
        return -1;
    }
    if (launcher_add_entry(&tmp) != 0) {
        g_launcher.ready = 0u;
        return -1;
    }

    if (mde_launcher_parse_manifest(g_builtin_files_manifest, &tmp) != 0) {
        g_launcher.ready = 0u;
        return -1;
    }
    if (launcher_add_entry(&tmp) != 0) {
        g_launcher.ready = 0u;
        return -1;
    }

    return 0;
}

void mde_launcher_shutdown(void)
{
    memset(&g_launcher, 0, sizeof(g_launcher));
    memset(&g_launcher_ui, 0, sizeof(g_launcher_ui));
}

uint32_t mde_launcher_entry_count(void)
{
    return g_launcher.ready ? g_launcher.count : 0u;
}

const mde_launcher_entry_t *mde_launcher_get_entry(uint32_t index)
{
    if (!g_launcher.ready || index >= g_launcher.count) {
        return NULL;
    }
    return &g_launcher.entries[index];
}

const mde_launcher_entry_t *mde_launcher_find(const char *app_id)
{
    uint32_t i;

    if (!g_launcher.ready || app_id == NULL) {
        return NULL;
    }
    for (i = 0; i < g_launcher.count; i++) {
        if (strcmp(g_launcher.entries[i].id, app_id) == 0) {
            return &g_launcher.entries[i];
        }
    }
    return NULL;
}

/*
 * Futuro: syscall METTA_SYS_APP_LAUNCH(app_id) -> app_launch en kernel.
 * Ahora devuelve 0 si el id existe, -1 si no (sin levantar proceso).
 */
int mde_launcher_spawn(const char *app_id)
{
    const mde_launcher_entry_t *e;

    if (!g_launcher.ready) {
        return -1;
    }
    e = mde_launcher_find(app_id);
    if (e == NULL) {
        return -1;
    }
    (void)e;
    return 0;
}

int mde_launcher_ingest_manifest_text(const char *text)
{
    mde_launcher_entry_t tmp;

    if (!g_launcher.ready) {
        return -1;
    }
    if (mde_launcher_parse_manifest(text, &tmp) != 0) {
        return -1;
    }
    return launcher_add_entry(&tmp);
}

void mde_launcher_ui_open(int32_t screen_w, int32_t screen_h)
{
    g_launcher_ui.open      = 1;
    g_launcher_ui.anim_armed = 1;
    g_launcher_ui.screen_w  = screen_w;
    g_launcher_ui.screen_h  = screen_h;
    g_launcher_ui.hover_index = -1;
    g_launcher_ui.query[0]  = '\0';
    launcher_ui_rebuild_filter();
    launcher_ui_relayout();
    mde_anim_overlay_reset_closed(MDE_ANIM_CH_LAUNCHER_FADE, MDE_ANIM_CH_LAUNCHER_SCALE);
}

void mde_launcher_ui_close(void)
{
    g_launcher_ui.open       = 0;
    g_launcher_ui.anim_armed = 0;
    g_launcher_ui.hover_index = -1;
    mde_anim_overlay_reset_closed(MDE_ANIM_CH_LAUNCHER_FADE, MDE_ANIM_CH_LAUNCHER_SCALE);
}

int mde_launcher_ui_is_open(void)
{
    return g_launcher_ui.open;
}

void mde_launcher_ui_tick(uint64_t now_ms)
{
    if (!g_launcher_ui.open) {
        return;
    }
    if (g_launcher_ui.anim_armed) {
        mde_anim_launcher_enter(now_ms);
        g_launcher_ui.anim_armed = 0;
    }
    (void)mde_anim_tick(now_ms);
}

void mde_launcher_ui_set_query(const char *text)
{
    size_t n;

    if (text == NULL) {
        g_launcher_ui.query[0] = '\0';
    } else {
        n = strlen(text);
        if (n >= sizeof(g_launcher_ui.query)) {
            n = sizeof(g_launcher_ui.query) - 1u;
        }
        memcpy(g_launcher_ui.query, text, n);
        g_launcher_ui.query[n] = '\0';
    }
    launcher_ui_rebuild_filter();
    launcher_ui_relayout();
}

const char *mde_launcher_ui_query(void)
{
    return g_launcher_ui.query;
}

static int32_t launcher_ui_cell_at(int32_t x, int32_t y)
{
    int32_t gx = g_launcher_ui.layout.grid_x;
    int32_t gy = g_launcher_ui.layout.grid_y;
    uint32_t cell = g_launcher_ui.layout.grid_cell;
    int32_t col;
    int32_t row;

    if (x < gx || y < gy) {
        return -1;
    }
    col = (x - gx) / (int32_t)cell;
    row = (y - gy) / (int32_t)cell;
    if (col < 0 || row < 0) {
        return -1;
    }
    if ((uint32_t)col >= g_launcher_ui.layout.grid_cols) {
        return -1;
    }
    {
        uint32_t idx = (uint32_t)row * g_launcher_ui.layout.grid_cols + (uint32_t)col;
        if (idx >= g_launcher_ui.visible_count) {
            return -1;
        }
        return (int32_t)idx;
    }
}

void mde_launcher_ui_pointer(int32_t x, int32_t y, int pressed)
{
    (void)pressed;
    if (!g_launcher_ui.open) {
        return;
    }
    g_launcher_ui.hover_index = launcher_ui_cell_at(x, y);
}

void mde_launcher_ui_layout(mde_launcher_ui_layout_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = g_launcher_ui.layout;
}

void mde_launcher_ui_panel_glass(mde_glass_surface_style *out)
{
    if (out == NULL) {
        return;
    }
    *out = g_launcher_ui.panel_glass;
}

void mde_launcher_ui_search_glass(mde_glass_surface_style *out)
{
    if (out == NULL) {
        return;
    }
    *out = g_launcher_ui.search_glass;
}

int32_t mde_launcher_ui_open_opacity(void)
{
    return mde_anim_launcher_opacity();
}

int32_t mde_launcher_ui_open_scale(void)
{
    return mde_anim_launcher_scale();
}

int32_t mde_launcher_ui_hovered_index(void)
{
    return g_launcher_ui.hover_index;
}

const mde_launcher_entry_t *mde_launcher_ui_visible_entry(uint32_t index)
{
    uint32_t src;

    if (!g_launcher_ui.open || index >= g_launcher_ui.visible_count) {
        return NULL;
    }
    src = g_launcher_ui.visible_map[index];
    return mde_launcher_get_entry(src);
}
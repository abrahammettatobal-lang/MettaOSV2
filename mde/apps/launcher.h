#ifndef MDE_LAUNCHER_H
#define MDE_LAUNCHER_H

#include <stddef.h>
#include <stdint.h>

#include "../theme/controls.h"

typedef enum {
    MDE_APP_KIND_UNKNOWN = 0,
    MDE_APP_KIND_GUI,
    MDE_APP_KIND_CLI,
    MDE_APP_KIND_SERVICE
} mde_app_kind_t;

typedef struct {
    char            id[64];
    char            name[128];
    char            version[32];
    char            entry[256];
    char            icon[256];
    mde_app_kind_t  kind;
} mde_launcher_entry_t;

typedef struct {
    int32_t  panel_x;
    int32_t  panel_y;
    int32_t  panel_w;
    int32_t  panel_h;
    int32_t  search_x;
    int32_t  search_y;
    int32_t  search_w;
    int32_t  search_h;
    int32_t  grid_x;
    int32_t  grid_y;
    uint32_t grid_cols;
    uint32_t grid_cell;
    uint32_t grid_rows;
    uint32_t visible_count;
} mde_launcher_ui_layout_t;

int  mde_launcher_init(void);
void mde_launcher_shutdown(void);
uint32_t mde_launcher_entry_count(void);
const mde_launcher_entry_t *mde_launcher_get_entry(uint32_t index);
const mde_launcher_entry_t *mde_launcher_find(const char *app_id);
int  mde_launcher_spawn(const char *app_id);
int  mde_launcher_ingest_manifest_text(const char *text);
int  mde_launcher_parse_manifest(const char *text, mde_launcher_entry_t *out);

void mde_launcher_ui_open(int32_t screen_w, int32_t screen_h);
void mde_launcher_ui_close(void);
int  mde_launcher_ui_is_open(void);
void mde_launcher_ui_tick(uint64_t now_ms);
void mde_launcher_ui_pointer(int32_t x, int32_t y, int pressed);
void mde_launcher_ui_set_query(const char *text);
const char *mde_launcher_ui_query(void);

void mde_launcher_ui_layout(mde_launcher_ui_layout_t *out);
void mde_launcher_ui_panel_glass(mde_glass_surface_style *out);
void mde_launcher_ui_search_glass(mde_glass_surface_style *out);
int32_t mde_launcher_ui_open_opacity(void);
int32_t mde_launcher_ui_open_scale(void);
int32_t mde_launcher_ui_hovered_index(void);
const mde_launcher_entry_t *mde_launcher_ui_visible_entry(uint32_t index);

#endif /* MDE_LAUNCHER_H */
/*
 * MettaOS MDE — compositor con doble buffer y present parcial (SA-15).
 * Contrato framebuffer: docs/swarm/sa-14-report.md
 */
#ifndef MDE_COMPOSITOR_H
#define MDE_COMPOSITOR_H

#include "compositor_mde.h"
#include "dirty.h"

#include <graphics/fb.h>
#include <stdint.h>

typedef struct {
    uint64_t frames;
    uint64_t present_rects_total;
    uint32_t last_damage_count;
    uint32_t last_present_count;
    uint64_t full_present_count;
} compositor_stats_t;

/* Repinta solo la región dañada en el back buffer. */
typedef void (*compositor_draw_region_fn)(void *ctx, int32_t x, int32_t y,
                                          int32_t w, int32_t h,
                                          uint32_t *backbuf, int32_t stride_px);

typedef void (*compositor_log_fn)(const char *line);

typedef struct {
    uint16_t cursor_w;
    uint16_t cursor_h;
    int      log_dirty_counts; /* escribe stats vía log_fn cada present */
    compositor_log_fn log_fn;
} compositor_config_t;

typedef struct {
    const framebuffer_t *fb;
    uint32_t            *back;
    int32_t              stride_px;
    uint32_t             width;
    uint32_t             height;

    comp_damage_t        damage;
    comp_damage_t        present_damage;

    int                  ready;
    compositor_config_t  cfg;

    /* Cursor software: solo rects pequeños al mover. */
    int                  cursor_visible;
    int32_t              cursor_x;
    int32_t              cursor_y;
    fb_rect_t            cursor_rect;
    fb_rect_t            cursor_prev;

    compositor_stats_t   stats;

    /* Bucle de frame (SA-13 / SA-17 / SA-20 / SA-22) */
    int32_t              pointer_x;
    int32_t              pointer_y;
    int32_t              last_hover_icon_id;
    int32_t              ctx_last_hover;
    int                  chrome_seeded;
    mde_desktop_icon_t   icons[MDE_DESKTOP_MAX_ICONS];
    uint32_t             icon_count;
    uint64_t             last_frame_ms;
} compositor_t;

/*
 * back_pixels: buffer CPU width×height (XRGB8888), provisto por PMM/caller.
 * Tras init, el primer frame requiere compositor_damage_full() o
 * compositor_begin_frame(..., 1).
 */
int compositor_init(compositor_t *c, const framebuffer_t *fb,
                    uint32_t *back_pixels, const compositor_config_t *cfg);

void compositor_shutdown(compositor_t *c);

/* Si invalidate_all, marca frame completo (p. ej. resize / primer pintado). */
void compositor_begin_frame(compositor_t *c, int invalidate_all);

void compositor_damage_rect(compositor_t *c, int32_t x, int32_t y,
                            int32_t w, int32_t h);
void compositor_damage_full(compositor_t *c);

uint32_t *compositor_back_buffer(compositor_t *c);
int32_t   compositor_stride(compositor_t *c);
const compositor_stats_t *compositor_stats(const compositor_t *c);

/* Invoca draw por cada rect dañado (o una vez pantalla completa). */
void compositor_render(compositor_t *c, compositor_draw_region_fn draw, void *ctx);

/*
 * Copia back→front solo en rects de present_damage; llama fb_present() una vez.
 * No hace blit de pantalla completa salvo full_frame.
 */
void compositor_present(compositor_t *c);

/*
 * Mueve cursor: invalida rect anterior + nuevo (típ. 16×16), sin full redraw.
 * No dibuja en back buffer; llamar render() y luego compositor_cursor_paint().
 */
void compositor_cursor_move(compositor_t *c, int32_t x, int32_t y);

void compositor_cursor_paint(compositor_t *c, const uint32_t *pixels,
                             int32_t sprite_stride_px);

void compositor_cursor_hide(compositor_t *c);

/* Repinta solo el rect del cursor en el back buffer (sin present completo). */
void compositor_cursor_restore_under(compositor_t *c, uint32_t desktop_color);

/*
 * Inicializa dock/topbar/viewport e interacción SA-22 (tras mde_init).
 * Requiere mde_phase READY.
 */
int compositor_shell_bootstrap(compositor_t *c);

void compositor_set_desktop_icons(compositor_t *c,
                                  const mde_desktop_icon_t *icons,
                                  uint32_t count);

/*
 * Ciclo completo: input_poll/input_pop, mde_desktop_tick, ticks de interacción,
 * dirty rects parciales, render, cursor, present. Sin full redraw por ratón.
 */
void compositor_frame_tick(compositor_t *c, uint64_t now_ms);

/* Presenta la escena ya, aunque el bucle de frames esté parado dentro de Firefox. */
void compositor_flush_now(void);

#endif /* MDE_COMPOSITOR_H */
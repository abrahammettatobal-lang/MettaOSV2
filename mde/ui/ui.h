/*
 * MettaOS — shell de escritorio (estilo Tahoe, Liquid Glass).
 * Entrada pública usada por el compositor.
 */
#ifndef MDE_UI_H
#define MDE_UI_H

#include <input/input.h>
#include <stdint.h>

int  ui_init(uint32_t *scene, int32_t width, int32_t height);
void ui_event(const input_event_t *ev, uint64_t now_ms);
void ui_tick(uint64_t now_ms);

/* Repinta la escena si hay daño; devuelve 1 y el rect pintado. */
int  ui_render(int32_t *x, int32_t *y, int32_t *w, int32_t *h);

/* Cursor con alfa sobre el back buffer (tras copiar la escena). */
void ui_cursor_draw(uint32_t *back, int32_t stride, int32_t width, int32_t height,
                    int32_t x, int32_t y);

#define UI_CURSOR_W 22
#define UI_CURSOR_H 26

#endif /* MDE_UI_H */

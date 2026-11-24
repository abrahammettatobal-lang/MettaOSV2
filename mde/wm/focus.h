#ifndef MDE_WM_FOCUS_H
#define MDE_WM_FOCUS_H

#include <stdint.h>

void wm_focus_init(void);

uint32_t wm_focus_get(void);
int wm_focus_set(uint32_t id);
int wm_focus_clear(void);

/* Ciclo en orden Z (de arriba hacia abajo). */
int wm_focus_next(void);
int wm_focus_prev(void);

int wm_focus_on_click(uint32_t id);

#endif /* MDE_WM_FOCUS_H */
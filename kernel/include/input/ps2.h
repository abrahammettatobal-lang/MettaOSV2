/*
 * MettaOS — driver PS/2 teclado y ratón (polling). SA-13
 */
#ifndef METTA_INPUT_PS2_H
#define METTA_INPUT_PS2_H

#include <stdint.h>

int  ps2_init(void);
void ps2_poll(void);

/** Limita el cursor a [0,w)×[0,h) (pantalla). */
void ps2_set_screen_bounds(int32_t width, int32_t height);
void ps2_set_position(int32_t x, int32_t y);
void ps2_get_position(int32_t *x, int32_t *y);

#endif /* METTA_INPUT_PS2_H */
/*
 * MettaOS — contrato Input ↔ MDE. SA-13
 * Ver docs/swarm/sa-13-report.md
 */
#ifndef METTA_INPUT_H
#define METTA_INPUT_H

#include <stddef.h>
#include <stdint.h>

#define INPUT_QUEUE_CAPACITY 128u

typedef enum {
    INPUT_EV_NONE = 0,
    INPUT_EV_KEY_DOWN,
    INPUT_EV_KEY_UP,
    INPUT_EV_MOUSE_MOVE,
    INPUT_EV_MOUSE_BUTTON_DOWN,
    INPUT_EV_MOUSE_BUTTON_UP,
    INPUT_EV_MOUSE_WHEEL,
} input_event_type_t;

typedef enum {
    INPUT_MOD_NONE  = 0,
    INPUT_MOD_SHIFT = 1u << 0,
    INPUT_MOD_CTRL  = 1u << 1,
    INPUT_MOD_ALT   = 1u << 2,
    INPUT_MOD_SUPER = 1u << 3,
} input_modifier_t;

typedef enum {
    INPUT_BTN_LEFT   = 0,
    INPUT_BTN_RIGHT  = 1,
    INPUT_BTN_MIDDLE = 2,
} input_mouse_button_t;

typedef enum {
    INPUT_KEY_UNKNOWN = 0,
    INPUT_KEY_ESC,
    INPUT_KEY_1,
    INPUT_KEY_2,
    INPUT_KEY_3,
    INPUT_KEY_4,
    INPUT_KEY_5,
    INPUT_KEY_6,
    INPUT_KEY_7,
    INPUT_KEY_8,
    INPUT_KEY_9,
    INPUT_KEY_0,
    INPUT_KEY_MINUS,
    INPUT_KEY_EQUAL,
    INPUT_KEY_BACKSPACE,
    INPUT_KEY_TAB,
    INPUT_KEY_Q,
    INPUT_KEY_W,
    INPUT_KEY_E,
    INPUT_KEY_R,
    INPUT_KEY_T,
    INPUT_KEY_Y,
    INPUT_KEY_U,
    INPUT_KEY_I,
    INPUT_KEY_O,
    INPUT_KEY_P,
    INPUT_KEY_LEFTBRACE,
    INPUT_KEY_RIGHTBRACE,
    INPUT_KEY_ENTER,
    INPUT_KEY_LEFTCTRL,
    INPUT_KEY_A,
    INPUT_KEY_S,
    INPUT_KEY_D,
    INPUT_KEY_F,
    INPUT_KEY_G,
    INPUT_KEY_H,
    INPUT_KEY_J,
    INPUT_KEY_K,
    INPUT_KEY_L,
    INPUT_KEY_SEMICOLON,
    INPUT_KEY_APOSTROPHE,
    INPUT_KEY_GRAVE,
    INPUT_KEY_LEFTSHIFT,
    INPUT_KEY_BACKSLASH,
    INPUT_KEY_Z,
    INPUT_KEY_X,
    INPUT_KEY_C,
    INPUT_KEY_V,
    INPUT_KEY_B,
    INPUT_KEY_N,
    INPUT_KEY_M,
    INPUT_KEY_COMMA,
    INPUT_KEY_DOT,
    INPUT_KEY_SLASH,
    INPUT_KEY_RIGHTSHIFT,
    INPUT_KEY_LEFTALT,
    INPUT_KEY_SPACE,
    INPUT_KEY_CAPSLOCK,
    INPUT_KEY_F1,
    INPUT_KEY_F2,
    INPUT_KEY_F3,
    INPUT_KEY_F4,
    INPUT_KEY_F5,
    INPUT_KEY_F6,
    INPUT_KEY_F7,
    INPUT_KEY_F8,
    INPUT_KEY_F9,
    INPUT_KEY_F10,
    INPUT_KEY_F11,
    INPUT_KEY_F12,
    INPUT_KEY_UP,
    INPUT_KEY_DOWN,
    INPUT_KEY_LEFT,
    INPUT_KEY_RIGHT,
    INPUT_KEY_HOME,
    INPUT_KEY_END,
    INPUT_KEY_PAGEUP,
    INPUT_KEY_PAGEDOWN,
    INPUT_KEY_INSERT,
    INPUT_KEY_DELETE,
    INPUT_KEY_RIGHTCTRL,
    INPUT_KEY_RIGHTALT,
} input_keycode_t;

typedef struct {
    input_event_type_t type;
    uint64_t           time_ms;

    union {
        struct {
            input_keycode_t   keycode;
            uint8_t           scancode;
            uint8_t           modifiers; /* bitmask input_modifier_t */
        } key;
        struct {
            int32_t x;
            int32_t y;
            int32_t dx;
            int32_t dy;
        } mouse;
        struct {
            input_mouse_button_t button;
            int8_t               wheel_delta;
        } btn;
    } u;
} input_event_t;

int  input_init(void);
void input_poll(void);
int  input_pop(input_event_t *out);
size_t input_pending(void);

#endif /* METTA_INPUT_H */
#ifndef VOS_INPUT_H
#define VOS_INPUT_H

#include <stdint.h>

enum input_event_type {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY = 1,
    INPUT_EVENT_MOUSE = 2
};

enum input_key_code {
    INPUT_KEY_ENTER = 0x01c,
    INPUT_KEY_ESCAPE = 0x001,
    INPUT_KEY_TAB = 0x00f,
    INPUT_KEY_BACKSPACE = 0x00e,
    INPUT_KEY_LEFT = 0x14b,
    INPUT_KEY_RIGHT = 0x14d,
    INPUT_KEY_UP = 0x148,
    INPUT_KEY_DOWN = 0x150,
    INPUT_KEY_HOME = 0x147,
    INPUT_KEY_END = 0x14f,
    INPUT_KEY_PAGE_UP = 0x149,
    INPUT_KEY_PAGE_DOWN = 0x151,
    INPUT_KEY_DELETE = 0x153,
    INPUT_KEY_LEFT_SHIFT = 0x02a,
    INPUT_KEY_RIGHT_SHIFT = 0x036,
    INPUT_KEY_CONTROL = 0x01d,
    INPUT_KEY_ALT = 0x038,
    INPUT_KEY_CAPS_LOCK = 0x03a
};

enum input_modifier {
    INPUT_MOD_SHIFT = 1u << 0,
    INPUT_MOD_CONTROL = 1u << 1,
    INPUT_MOD_ALT = 1u << 2,
    INPUT_MOD_CAPS_LOCK = 1u << 3
};

enum input_mouse_button {
    INPUT_MOUSE_LEFT = 1u << 0,
    INPUT_MOUSE_RIGHT = 1u << 1,
    INPUT_MOUSE_MIDDLE = 1u << 2
};

struct input_event {
    enum input_event_type type;
    uint16_t key;
    uint8_t pressed;
    uint8_t modifiers;
    uint32_t character;
    int32_t mouse_x;
    int32_t mouse_y;
    int16_t mouse_delta_x;
    int16_t mouse_delta_y;
    uint8_t mouse_buttons;
    uint8_t mouse_buttons_changed;
};

/* Initializes legacy PS/2 input. Mouse support is optional. */
int input_init(uint64_t screen_width, uint64_t screen_height);
int input_controller_available(void);
int input_mouse_available(void);

/* Non-blocking: returns 1 when an event was produced, 0 otherwise. */
int input_poll_event(struct input_event *event);

#endif

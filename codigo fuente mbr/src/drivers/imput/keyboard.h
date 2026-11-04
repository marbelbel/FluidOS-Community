#ifndef VOS_KEYBOARD_H
#define VOS_KEYBOARD_H

#include "input.h"
#include <stdint.h>

enum keyboard_layout {
    KEYBOARD_LAYOUT_SPANISH = 0,
    KEYBOARD_LAYOUT_US = 1,
    KEYBOARD_LAYOUT_UK = 2,
    KEYBOARD_LAYOUT_FRENCH = 3,
    KEYBOARD_LAYOUT_GERMAN = 4,
    KEYBOARD_LAYOUT_COUNT
};

void keyboard_set_layout(enum keyboard_layout layout);
void keyboard_toggle_layout(void);
enum keyboard_layout keyboard_current_layout(void);
const char *keyboard_layout_name(void);
int keyboard_process_scancode(uint8_t scan_code, struct input_event *event);

#endif

#include "input.h"
#include "keyboard.h"
#include "mouse.h"
#include "ps2.h"

#define INPUT_BYTE_LIMIT 32

static int controller_available;

int input_init(uint64_t screen_width, uint64_t screen_height) {
    controller_available = 0;
    (void)mouse_init(0, 0);
    if (!ps2_init()) {
        return 0;
    }
    controller_available = 1;
    (void)mouse_init(screen_width, screen_height);
    return 1;
}

int input_controller_available(void) {
    return controller_available;
}

int input_mouse_available(void) {
    return mouse_is_available();
}

int input_poll_event(struct input_event *event) {
    if (event == 0 || !controller_available) {
        return 0;
    }

    event->type = INPUT_EVENT_NONE;
    event->key = 0;
    event->pressed = 0;
    event->modifiers = 0;
    event->character = 0;
    event->mouse_x = 0;
    event->mouse_y = 0;
    event->mouse_delta_x = 0;
    event->mouse_delta_y = 0;
    event->mouse_buttons = 0;
    event->mouse_buttons_changed = 0;

    for (uint32_t i = 0; i < INPUT_BYTE_LIMIT; i++) {
        uint8_t value;
        enum ps2_device device;
        if (!ps2_read_byte(&value, &device)) {
            return 0;
        }

        if (device == PS2_DEVICE_MOUSE) {
            if (mouse_process_byte(value, event)) {
                return 1;
            }
        } else if (keyboard_process_scancode(value, event)) {
            return 1;
        }
    }
    return 0;
}

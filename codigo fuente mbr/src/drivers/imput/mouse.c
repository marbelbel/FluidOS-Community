#include "mouse.h"
#include "ps2.h"

static uint8_t packet[3];
static uint8_t packet_index;
static uint8_t buttons;
static int32_t cursor_x;
static int32_t cursor_y;
static uint64_t screen_width;
static uint64_t screen_height;
static int available;

int mouse_init(uint64_t width, uint64_t height) {
    available = 0;
    packet_index = 0;
    buttons = 0;
    screen_width = width;
    screen_height = height;

    if (width == 0 || height == 0 || width > INT32_MAX || height > INT32_MAX) {
        return 0;
    }

    cursor_x = (int32_t)(width / 2);
    cursor_y = (int32_t)(height / 2);

    if (!ps2_mouse_command(0xf6) || !ps2_mouse_command(0xf4)) {
        return 0;
    }
    available = 1;
    return 1;
}

int mouse_is_available(void) {
    return available;
}

int mouse_process_byte(uint8_t byte, struct input_event *event) {
    if (!available || event == 0) {
        return 0;
    }

    if (packet_index == 0) {
        /* Bit 3 is always set in the first byte; it resynchronizes the stream. */
        if ((byte & 0x08) == 0) {
            return 0;
        }
        packet[packet_index++] = byte;
        return 0;
    }

    packet[packet_index++] = byte;
    if (packet_index < sizeof(packet)) {
        return 0;
    }
    packet_index = 0;

    int16_t delta_x = (int8_t)packet[1];
    int16_t delta_y = -(int16_t)(int8_t)packet[2];
    if ((packet[0] & 0xc0) != 0) {
        delta_x = 0;
        delta_y = 0;
    }

    int64_t next_x = (int64_t)cursor_x + delta_x;
    int64_t next_y = (int64_t)cursor_y + delta_y;
    if (next_x < 0) next_x = 0;
    if (next_y < 0) next_y = 0;
    if ((uint64_t)next_x >= screen_width) next_x = (int64_t)screen_width - 1;
    if ((uint64_t)next_y >= screen_height) next_y = (int64_t)screen_height - 1;

    uint8_t new_buttons = packet[0] & 0x07;
    event->type = INPUT_EVENT_MOUSE;
    event->mouse_delta_x = delta_x;
    event->mouse_delta_y = delta_y;
    event->mouse_x = cursor_x = (int32_t)next_x;
    event->mouse_y = cursor_y = (int32_t)next_y;
    event->mouse_buttons = new_buttons;
    event->mouse_buttons_changed = buttons ^ new_buttons;
    buttons = new_buttons;
    return 1;
}

#ifndef VOS_PS2_H
#define VOS_PS2_H

#include <stdint.h>

enum ps2_device {
    PS2_DEVICE_KEYBOARD = 0,
    PS2_DEVICE_MOUSE = 1
};

int ps2_init(void);
int ps2_read_byte(uint8_t *value, enum ps2_device *device);
int ps2_mouse_command(uint8_t command);

#endif

#ifndef VOS_MOUSE_H
#define VOS_MOUSE_H

#include "input.h"

int mouse_init(uint64_t screen_width, uint64_t screen_height);
int mouse_process_byte(uint8_t byte, struct input_event *event);
int mouse_is_available(void);

#endif

#ifndef VOS_GRAPHICS_H
#define VOS_GRAPHICS_H

#include <stdint.h>
#include "limine.h"

struct graphics_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

int graphics_init(struct limine_framebuffer *framebuffer);
uint64_t graphics_width(void);
uint64_t graphics_height(void);
void graphics_clear(struct graphics_color color);
void graphics_fill_rect(uint64_t x, uint64_t y, uint64_t width,
                        uint64_t height, struct graphics_color color);
void graphics_fill_round_rect(uint64_t x, uint64_t y, uint64_t width,
                              uint64_t height, uint32_t radius,
                              struct graphics_color color);
void graphics_draw_char(char character, uint64_t x, uint64_t y,
                        uint32_t scale, struct graphics_color color);
void graphics_draw_text(const char *text, uint64_t x, uint64_t y,
                        uint32_t scale, struct graphics_color color);
void graphics_scroll_rows_up(uint64_t y, uint64_t height, uint64_t rows,
                             struct graphics_color fill_color);

#endif

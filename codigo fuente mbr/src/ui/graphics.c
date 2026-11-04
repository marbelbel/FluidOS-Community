#include "graphics.h"

static struct limine_framebuffer *screen;
static uint8_t pixel_bytes;

static uint64_t channel_mask(uint8_t size, uint8_t shift) {
    return ((1ULL << size) - 1ULL) << shift;
}

int graphics_init(struct limine_framebuffer *framebuffer) {
    screen = 0;
    pixel_bytes = 0;

    if (framebuffer == 0 || framebuffer->address == 0 ||
        framebuffer->width == 0 || framebuffer->height == 0 ||
        framebuffer->pitch == 0 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        (framebuffer->bpp != 15 && framebuffer->bpp != 16 &&
         framebuffer->bpp != 24 && framebuffer->bpp != 32) ||
        framebuffer->red_mask_size == 0 || framebuffer->red_mask_size > 8 ||
        framebuffer->green_mask_size == 0 || framebuffer->green_mask_size > 8 ||
        framebuffer->blue_mask_size == 0 || framebuffer->blue_mask_size > 8 ||
        (uint16_t)framebuffer->red_mask_shift + framebuffer->red_mask_size > framebuffer->bpp ||
        (uint16_t)framebuffer->green_mask_shift + framebuffer->green_mask_size > framebuffer->bpp ||
        (uint16_t)framebuffer->blue_mask_shift + framebuffer->blue_mask_size > framebuffer->bpp) {
        return 0;
    }

    uint64_t red_mask = channel_mask(framebuffer->red_mask_size,
                                     framebuffer->red_mask_shift);
    uint64_t green_mask = channel_mask(framebuffer->green_mask_size,
                                       framebuffer->green_mask_shift);
    uint64_t blue_mask = channel_mask(framebuffer->blue_mask_size,
                                      framebuffer->blue_mask_shift);
    if ((red_mask & green_mask) != 0 || (red_mask & blue_mask) != 0 ||
        (green_mask & blue_mask) != 0) {
        return 0;
    }

    uint64_t bytes_per_pixel = (framebuffer->bpp + 7) / 8;
    if (framebuffer->width > UINT64_MAX / bytes_per_pixel ||
        framebuffer->pitch < framebuffer->width * bytes_per_pixel ||
        framebuffer->height > UINT64_MAX / framebuffer->pitch) {
        return 0;
    }

    uint64_t framebuffer_bytes = framebuffer->height * framebuffer->pitch;
    if ((uintptr_t)framebuffer->address > UINTPTR_MAX - framebuffer_bytes) {
        return 0;
    }

    screen = framebuffer;
    pixel_bytes = (uint8_t)bytes_per_pixel;
    return 1;
}

uint64_t graphics_width(void) {
    return screen == 0 ? 0 : screen->width;
}

uint64_t graphics_height(void) {
    return screen == 0 ? 0 : screen->height;
}

static uint32_t pack_color(struct graphics_color color) {
    uint32_t red = color.red >> (8 - screen->red_mask_size);
    uint32_t green = color.green >> (8 - screen->green_mask_size);
    uint32_t blue = color.blue >> (8 - screen->blue_mask_size);

    return (red << screen->red_mask_shift) |
           (green << screen->green_mask_shift) |
           (blue << screen->blue_mask_shift);
}

static void put_pixel(uint64_t x, uint64_t y, uint32_t packed_color) {
    if (screen == 0 || x >= screen->width || y >= screen->height) {
        return;
    }

    uint64_t offset = y * screen->pitch + x * pixel_bytes;
    volatile uint8_t *target = (volatile uint8_t *)
        ((uintptr_t)screen->address + offset);
    for (uint8_t byte = 0; byte < pixel_bytes; byte++) {
        target[byte] = (uint8_t)(packed_color >> (byte * 8));
    }
}

void graphics_clear(struct graphics_color color) {
    if (screen != 0) {
        graphics_fill_rect(0, 0, screen->width, screen->height, color);
    }
}

void graphics_fill_rect(uint64_t x, uint64_t y, uint64_t width,
                        uint64_t height, struct graphics_color color) {
    if (screen == 0 || x >= screen->width || y >= screen->height ||
        width == 0 || height == 0) {
        return;
    }

    if (width > screen->width - x) {
        width = screen->width - x;
    }
    if (height > screen->height - y) {
        height = screen->height - y;
    }

    uint32_t packed_color = pack_color(color);
    for (uint64_t row = 0; row < height; row++) {
        volatile uint8_t *target = (volatile uint8_t *)
            ((uintptr_t)screen->address + (y + row) * screen->pitch +
             x * pixel_bytes);
        for (uint64_t column = 0; column < width; column++) {
            volatile uint8_t *pixel = target + column * pixel_bytes;
            for (uint8_t byte = 0; byte < pixel_bytes; byte++) {
                pixel[byte] = (uint8_t)(packed_color >> (byte * 8));
            }
        }
    }
}

static uint64_t integer_sqrt(uint64_t value) {
    uint64_t root = 0;
    uint64_t bit = 1ULL << 62;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

void graphics_fill_round_rect(uint64_t x, uint64_t y, uint64_t width,
                              uint64_t height, uint32_t radius,
                              struct graphics_color color) {
    if (screen == 0 || width == 0 || height == 0 ||
        x >= screen->width || y >= screen->height) {
        return;
    }
    if (width > screen->width - x) width = screen->width - x;
    if (height > screen->height - y) height = screen->height - y;
    if (radius > width / 2) radius = (uint32_t)(width / 2);
    if (radius > height / 2) radius = (uint32_t)(height / 2);
    if (radius > 32) radius = 32;
    if (radius == 0) {
        graphics_fill_rect(x, y, width, height, color);
        return;
    }

    uint64_t radius_squared = (uint64_t)radius * radius;
    for (uint64_t row = 0; row < height; row++) {
        uint64_t distance = 0;
        if (row < radius) {
            distance = radius - row;
        } else if (height - 1 - row < radius) {
            distance = radius - (height - 1 - row);
        }

        uint64_t inset = 0;
        if (distance != 0) {
            uint64_t inside = radius_squared - distance * distance;
            inset = radius - integer_sqrt(inside);
        }
        if (inset > width / 2 || x > UINT64_MAX - inset) continue;
        graphics_fill_rect(x + inset, y + row, width - 2 * inset, 1, color);
    }
}

static const uint8_t letters[26][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,2,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
};

static const uint8_t digits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};

static const uint8_t *glyph(uint32_t character) {
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    static const uint8_t period[7] = {0,0,0,0,0,12,12};
    static const uint8_t comma[7] = {0,0,0,0,0,4,8};
    static const uint8_t colon[7] = {0,12,12,0,12,12,0};
    static const uint8_t semicolon[7] = {0,4,4,0,4,4,8};
    static const uint8_t greater[7] = {16,8,4,2,4,8,16};
    static const uint8_t slash[7] = {1,2,2,4,8,8,16};
    static const uint8_t less[7] = {8,4,2,1,2,4,8};
    static const uint8_t exclamation[7] = {4,4,4,4,4,0,4};
    static const uint8_t question[7] = {14,17,1,2,4,0,4};
    static const uint8_t underscore[7] = {0,0,0,0,0,0,31};
    static const uint8_t plus[7] = {0,4,4,31,4,4,0};
    static const uint8_t equals[7] = {0,31,0,31,0,0,0};
    static const uint8_t left_parenthesis[7] = {2,4,8,8,8,4,2};
    static const uint8_t right_parenthesis[7] = {8,4,2,2,2,4,8};
    static const uint8_t left_bracket[7] = {14,8,8,8,8,8,14};
    static const uint8_t right_bracket[7] = {14,2,2,2,2,2,14};
    static const uint8_t backslash[7] = {16,8,8,4,2,2,1};
    static const uint8_t pipe[7] = {4,4,4,4,4,4,4};
    static const uint8_t asterisk[7] = {0,21,14,31,14,21,0};
    static const uint8_t percent[7] = {17,2,4,8,16,17,0};
    static const uint8_t ampersand[7] = {12,18,20,8,21,18,13};
    static const uint8_t hash[7] = {10,31,10,10,31,10,0};
    static const uint8_t at[7] = {14,17,23,21,23,16,14};
    static const uint8_t caret[7] = {4,10,17,0,0,0,0};
    static const uint8_t tilde[7] = {0,0,9,22,0,0,0};
    static const uint8_t apostrophe[7] = {4,4,2,0,0,0,0};
    static const uint8_t quote[7] = {10,10,5,0,0,0,0};
    static const uint8_t left_brace[7] = {3,4,4,8,4,4,3};
    static const uint8_t right_brace[7] = {24,4,4,2,4,4,24};
    static const uint8_t a_acute[7] = {4,2,14,17,31,17,17};
    static const uint8_t a_grave[7] = {2,4,14,17,31,17,17};
    static const uint8_t a_umlaut[7] = {10,0,14,17,31,17,17};
    static const uint8_t e_grave[7] = {2,4,31,16,30,16,31};
    static const uint8_t e_acute[7] = {4,2,31,16,30,16,31};
    static const uint8_t i_acute[7] = {4,2,14,4,4,4,14};
    static const uint8_t o_acute[7] = {4,2,14,17,17,17,14};
    static const uint8_t u_acute[7] = {4,2,17,17,17,17,14};
    static const uint8_t u_diaeresis[7] = {10,0,17,17,17,17,14};
    static const uint8_t u_grave[7] = {2,4,17,17,17,17,14};
    static const uint8_t o_umlaut[7] = {10,0,14,17,17,17,14};
    static const uint8_t sharp_s[7] = {14,17,17,30,17,17,30};
    static const uint8_t n_tilde[7] = {14,18,25,21,19,17,17};
    static const uint8_t inverted_exclamation[7] = {4,4,4,4,4,0,4};
    static const uint8_t inverted_question[7] = {4,0,4,8,16,17,14};
    static const uint8_t middle_dot[7] = {0,0,0,12,12,0,0};
    static const uint8_t cedilla[7] = {14,17,16,16,17,14,8};
    static const uint8_t masculine_ordinal[7] = {14,17,17,14,1,0,1};
    static const uint8_t feminine_ordinal[7] = {14,17,17,14,17,0,17};
    static const uint8_t acute_mark[7] = {8,4,2,0,0,0,0};
    static const uint8_t diaeresis_mark[7] = {10,0,0,0,0,0,0};
    static const uint8_t superscript_two[7] = {14,1,2,4,8,15,0};
    static const uint8_t superscript_three[7] = {14,1,6,1,1,14,0};
    static const uint8_t degree[7] = {6,9,9,6,0,0,0};
    static const uint8_t pound[7] = {6,9,8,30,8,8,31};
    static const uint8_t section[7] = {15,16,14,17,14,1,30};
    static const uint8_t micro[7] = {0,17,17,17,19,29,16};
    static const uint8_t not_sign[7] = {0,0,31,1,1,1,0};

    if (character >= 'a' && character <= 'z') {
        character = character - 'a' + 'A';
    }
    if ((character >= 0x00e0 && character <= 0x00f6) ||
        (character >= 0x00f8 && character <= 0x00fe)) {
        character -= 0x20;
    }
    if (character >= 'A' && character <= 'Z') {
        return letters[character - 'A'];
    }
    if (character >= '0' && character <= '9') {
        return digits[character - '0'];
    }

    switch (character) {
        case 0x00a1: return inverted_exclamation;
        case 0x00bf: return inverted_question;
        case 0x00b7: return middle_dot;
        case 0x00a3: return pound;
        case 0x00a7: return section;
        case 0x00ac: return not_sign;
        case 0x00a8: return diaeresis_mark;
        case 0x00b0: return degree;
        case 0x00b2: return superscript_two;
        case 0x00b3: return superscript_three;
        case 0x00b5: return micro;
        case 0x00b4: return acute_mark;
        case 0x00ba: return masculine_ordinal;
        case 0x00aa: return feminine_ordinal;
        case 0x00c7: return cedilla;
        case 0x00e7: return cedilla;
        case 0x00c0: return a_grave;
        case 0x00c4: return a_umlaut;
        case 0x00c1: return a_acute;
        case 0x00c8: return e_grave;
        case 0x00c9: return e_acute;
        case 0x00cd: return i_acute;
        case 0x00d3: return o_acute;
        case 0x00d6: return o_umlaut;
        case 0x00d9: return u_grave;
        case 0x00da: return u_acute;
        case 0x00dc: return u_diaeresis;
        case 0x00d1: return n_tilde;
        case 0x00df: return sharp_s;
        default: break;
    }

    switch (character) {
        case '-': return dash;
        case '.': return period;
        case ',': return comma;
        case ':': return colon;
        case ';': return semicolon;
        case '<': return less;
        case '>': return greater;
        case '/': return slash;
        case '!': return exclamation;
        case '?': return question;
        case '_': return underscore;
        case '+': return plus;
        case '=': return equals;
        case '(': return left_parenthesis;
        case ')': return right_parenthesis;
        case '[': return left_bracket;
        case ']': return right_bracket;
        case '\\': return backslash;
        case '|': return pipe;
        case '*': return asterisk;
        case '%': return percent;
        case '&': return ampersand;
        case '#': return hash;
        case '@': return at;
        case '^': return caret;
        case '~': return tilde;
        case '\'': return apostrophe;
        case '"': return quote;
        case '{': return left_brace;
        case '}': return right_brace;
        default: return blank;
    }
}

static void draw_codepoint(uint32_t codepoint, uint64_t x, uint64_t y,
                           uint32_t scale, struct graphics_color color) {
    if (screen == 0 || x >= screen->width || y >= screen->height ||
        scale == 0 || scale > 8 ||
        x > UINT64_MAX - 5 * scale || y > UINT64_MAX - 7 * scale) {
        return;
    }

    const uint8_t *rows = glyph(codepoint);
    uint32_t packed_color = pack_color(color);
    for (uint64_t row = 0; row < 7; row++) {
        for (uint64_t column = 0; column < 5; column++) {
            if ((rows[row] & (1u << (4 - column))) == 0) {
                continue;
            }
            for (uint32_t dy = 0; dy < scale; dy++) {
                for (uint32_t dx = 0; dx < scale; dx++) {
                    put_pixel(x + column * scale + dx,
                              y + row * scale + dy, packed_color);
                }
            }
        }
    }
}

void graphics_draw_char(char character, uint64_t x, uint64_t y,
                        uint32_t scale, struct graphics_color color) {
    draw_codepoint((uint8_t)character, x, y, scale, color);
}

static uint32_t decode_text_codepoint(const char **text) {
    uint8_t first = (uint8_t)*(*text)++;
    if (first < 0x80) {
        return first;
    }

    /* The built-in font supports the two-byte Latin-1 letters used in Spanish. */
    if (first >= 0xc2 && first <= 0xc3) {
        uint8_t second = (uint8_t)**text;
        if ((second & 0xc0) == 0x80) {
            (*text)++;
            return ((uint32_t)(first & 0x1f) << 6) | (second & 0x3f);
        }
    }
    return 0xfffd;
}

void graphics_draw_text(const char *text, uint64_t x, uint64_t y,
                        uint32_t scale, struct graphics_color color) {
    if (screen == 0 || text == 0 || scale == 0 || scale > 8 ||
        x >= screen->width || y >= screen->height ||
        screen->width <= 5 * scale || screen->height <= 8 * scale) {
        return;
    }

    const uint64_t start_x = x;
    const uint64_t advance_x = 6 * scale;
    const uint64_t advance_y = 8 * scale;
    while (*text != '\0') {
        if (*text == '\n') {
            if (y > screen->height - 1 - advance_y) {
                return;
            }
            y += advance_y;
            x = start_x;
            text++;
            continue;
        }

        if (x > screen->width - 1 - (5 * scale)) {
            if (y > screen->height - 1 - advance_y) {
                return;
            }
            y += advance_y;
            x = start_x;
        }

        uint32_t codepoint = decode_text_codepoint(&text);
        draw_codepoint(codepoint, x, y, scale, color);
        if (x > UINT64_MAX - advance_x) {
            return;
        }
        x += advance_x;
    }
}

void graphics_scroll_rows_up(uint64_t y, uint64_t height, uint64_t rows,
                             struct graphics_color fill_color) {
    if (screen == 0 || y >= screen->height || height == 0 || rows == 0) {
        return;
    }
    if (height > screen->height - y) {
        height = screen->height - y;
    }
    if (rows >= height) {
        graphics_fill_rect(0, y, screen->width, height, fill_color);
        return;
    }

    for (uint64_t row = 0; row < height - rows; row++) {
        volatile uint8_t *destination = (volatile uint8_t *)
            ((uintptr_t)screen->address + (y + row) * screen->pitch);
        volatile uint8_t *source = (volatile uint8_t *)
            ((uintptr_t)screen->address + (y + row + rows) * screen->pitch);
        for (uint64_t byte = 0; byte < screen->pitch; byte++) {
            destination[byte] = source[byte];
        }
    }
    graphics_fill_rect(0, y + height - rows, screen->width, rows, fill_color);
}

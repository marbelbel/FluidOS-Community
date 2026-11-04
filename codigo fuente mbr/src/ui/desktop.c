#include "desktop.h"
#include "apps.h"
#include "graphics.h"
#include "input.h"
#include "keyboard.h"
#include "locale.h"
#include "pmm.h"

struct desktop_palette {
    struct graphics_color sky_top;
    struct graphics_color sky_bottom;
    struct graphics_color bar;
    struct graphics_color dock;
    struct graphics_color window;
    struct graphics_color titlebar;
    struct graphics_color text;
    struct graphics_color muted;
    struct graphics_color accent;
    struct graphics_color white;
    struct graphics_color shadow;
    struct graphics_color success;
    struct graphics_color pending;
    struct graphics_color card;
};

struct desktop_state {
    uint64_t window_x;
    uint64_t window_y;
    uint64_t window_width;
    uint64_t window_height;
    uint64_t restore_x;
    uint64_t restore_y;
    uint64_t restore_width;
    uint64_t restore_height;
    int32_t mouse_x;
    int32_t mouse_y;
    int32_t drag_offset_x;
    int32_t drag_offset_y;
    enum vos_app_id active_app;
    uint8_t window_visible;
    uint8_t dragging;
    uint8_t maximized;
    uint8_t dark_mode;
    uint8_t accent_mode;
    uint8_t setup_active;
    uint8_t setup_step;
    uint8_t setup_selection;
};

static const struct desktop_palette light_palette = {
    {75, 150, 221}, {43, 70, 145}, {20, 28, 47}, {29, 39, 61},
    {247, 249, 253}, {224, 232, 243}, {32, 45, 63}, {89, 104, 125},
    {58, 129, 229}, {251, 253, 255}, {18, 26, 42}, {42, 142, 91},
    {182, 116, 30}, {232, 238, 247}
};

static const struct desktop_palette dark_palette = {
    {28, 51, 89}, {15, 21, 43}, {14, 19, 32}, {20, 28, 47},
    {37, 46, 63}, {49, 60, 79}, {232, 238, 247}, {173, 185, 202},
    {94, 157, 244}, {249, 251, 255}, {8, 12, 22}, {107, 211, 153},
    {255, 196, 102}, {51, 63, 83}
};

static const char *const dock_names[VOS_APP_COUNT] = {
    "INICIO", "SISTEMA", "MEMORIA", "CENTRO VOS", "AJUSTES", "AYUDA",
    "ARCHIVOS", "APPS", "CALC"
};
static const struct graphics_color dock_colors[VOS_APP_COUNT] = {
    {65, 132, 222}, {91, 111, 142}, {43, 145, 94},
    {136, 91, 222}, {221, 151, 51}, {38, 153, 164}, {68, 148, 130},
    {216, 85, 111}, {101, 116, 145}
};

static int point_in_rect(int32_t x, int32_t y, uint64_t left, uint64_t top,
                         uint64_t width, uint64_t height) {
    return x >= 0 && y >= 0 && (uint64_t)x >= left && (uint64_t)y >= top &&
           (uint64_t)x - left < width && (uint64_t)y - top < height;
}

static uint64_t text_length(const char *text) {
    uint64_t length = 0;
    while (text[length] != '\0') length++;
    return length;
}

static uint64_t desktop_integer_sqrt(uint64_t value) {
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

static uint8_t blend_channel(uint8_t background, uint8_t highlight,
                             uint32_t strength) {
    return (uint8_t)(((uint32_t)background * (255 - strength) +
                      (uint32_t)highlight * strength) / 255);
}

static struct graphics_color blend_color(struct graphics_color background,
                                         struct graphics_color highlight,
                                         uint32_t strength) {
    struct graphics_color result = {
        blend_channel(background.red, highlight.red, strength),
        blend_channel(background.green, highlight.green, strength),
        blend_channel(background.blue, highlight.blue, strength)
    };
    return result;
}

static struct graphics_color wallpaper_color_at(
    const struct desktop_palette *palette, uint64_t y, uint64_t top,
    uint64_t content_height) {
    uint64_t position = y > top ? y - top : 0;
    if (position > content_height) position = content_height;
    struct graphics_color color = {
        (uint8_t)((int64_t)palette->sky_top.red +
            ((int64_t)palette->sky_bottom.red - palette->sky_top.red) *
            (int64_t)position / (int64_t)content_height),
        (uint8_t)((int64_t)palette->sky_top.green +
            ((int64_t)palette->sky_bottom.green - palette->sky_top.green) *
            (int64_t)position / (int64_t)content_height),
        (uint8_t)((int64_t)palette->sky_top.blue +
            ((int64_t)palette->sky_bottom.blue - palette->sky_top.blue) *
            (int64_t)position / (int64_t)content_height)
    };
    return color;
}

static void draw_wallpaper_glow(uint64_t center_x, uint64_t center_y,
                                uint64_t radius_x, uint64_t radius_y,
                                uint32_t strength,
                                const struct desktop_palette *palette) {
    uint64_t height = graphics_height();
    uint64_t top = 28;
    if (radius_x == 0 || radius_y == 0 || height <= top ||
        strength == 0 || strength > 255) {
        return;
    }

    uint64_t radius_x_squared = radius_x * radius_x;
    uint64_t radius_y_squared = radius_y * radius_y;
    uint64_t first_y = center_y > radius_y ? center_y - radius_y : 0;
    uint64_t last_y = center_y + radius_y;
    if (last_y >= height) last_y = height - 1;
    struct graphics_color highlight = blend_color(
        palette->accent, (struct graphics_color){255, 255, 255}, 88);

    for (uint64_t y = first_y; y <= last_y; y++) {
        uint64_t distance_y = y > center_y ? y - center_y : center_y - y;
        if (distance_y >= radius_y) continue;
        uint64_t remaining = radius_y_squared - distance_y * distance_y;
        uint64_t span_squared = radius_x_squared * remaining / radius_y_squared;
        uint64_t span = desktop_integer_sqrt(span_squared);
        uint32_t center_weight = (uint32_t)(remaining * strength /
                                             radius_y_squared);
        struct graphics_color base = wallpaper_color_at(
            palette, y, top, height - top);
        struct graphics_color glow = blend_color(base, highlight, center_weight);
        uint64_t left = center_x > span ? center_x - span : 0;
        uint64_t width = span * 2 + 1;
        graphics_fill_rect(left, y, width, 1, glow);
    }
}

static void draw_cursor(int32_t x, int32_t y,
                        const struct desktop_palette *palette) {
    if (x < 0 || y < 0) return;
    for (uint64_t row = 0; row < 13; row++) {
        uint64_t span = row < 8 ? 1 + row / 2 : 5 - (row - 8) / 2;
        graphics_fill_rect((uint64_t)x + row / 2 + 1,
                           (uint64_t)y + row + 1, span + 1, 1, palette->shadow);
        graphics_fill_rect((uint64_t)x + row / 2,
                           (uint64_t)y + row, span, 1, palette->white);
    }
    graphics_fill_rect((uint64_t)x + 4, (uint64_t)y + 9, 4, 2, palette->shadow);
    graphics_fill_rect((uint64_t)x + 4, (uint64_t)y + 8, 2, 1, palette->white);
}

static void draw_window_control(uint64_t x, uint64_t y,
                                struct graphics_color color) {
    graphics_fill_rect(x + 2, y, 4, 1, color);
    graphics_fill_rect(x + 1, y + 1, 6, 1, color);
    graphics_fill_rect(x, y + 2, 8, 4, color);
    graphics_fill_rect(x + 1, y + 6, 6, 1, color);
    graphics_fill_rect(x + 2, y + 7, 4, 1, color);
}

static uint64_t dock_x(void) {
    const uint64_t spacing = graphics_width() < 450 ? 38 : 44;
    const uint64_t width = 8 + (uint64_t)VOS_APP_COUNT * spacing;
    return graphics_width() > width ? (graphics_width() - width) / 2 : 0;
}

static uint64_t dock_spacing(void) {
    return graphics_width() < 450 ? 38 : 44;
}

static uint64_t dock_y(void) {
    return graphics_height() > 60 ? graphics_height() - 60 : 0;
}

static void draw_dock_icon(uint32_t index, uint64_t x, uint64_t y,
                           struct graphics_color color) {
    const struct graphics_color white = {250, 252, 255};
    graphics_fill_round_rect(x, y, 36, 36, 9, color);

    switch (index) {
        case VOS_APP_HOME:
            graphics_fill_rect(x + 7, y + 14, 22, 16, white);
            graphics_fill_rect(x + 10, y + 11, 16, 3, white);
            graphics_fill_rect(x + 13, y + 8, 10, 3, white);
            graphics_fill_rect(x + 16, y + 5, 4, 3, white);
            graphics_fill_rect(x + 15, y + 22, 6, 8, color);
            break;
        case VOS_APP_SYSTEM:
            graphics_fill_rect(x + 6, y + 7, 24, 18, white);
            graphics_fill_rect(x + 9, y + 10, 18, 12, color);
            graphics_fill_rect(x + 16, y + 25, 4, 5, white);
            graphics_fill_rect(x + 11, y + 30, 14, 2, white);
            break;
        case VOS_APP_MEMORY:
            graphics_fill_rect(x + 11, y + 10, 14, 16, white);
            graphics_fill_rect(x + 14, y + 13, 8, 10, color);
            graphics_fill_rect(x + 14, y + 6, 2, 4, white);
            graphics_fill_rect(x + 20, y + 6, 2, 4, white);
            graphics_fill_rect(x + 14, y + 26, 2, 4, white);
            graphics_fill_rect(x + 20, y + 26, 2, 4, white);
            graphics_fill_rect(x + 7, y + 14, 4, 2, white);
            graphics_fill_rect(x + 7, y + 20, 4, 2, white);
            graphics_fill_rect(x + 25, y + 14, 4, 2, white);
            graphics_fill_rect(x + 25, y + 20, 4, 2, white);
            break;
        case VOS_APP_SERVICES:
            graphics_fill_rect(x + 7, y + 10, 22, 3, white);
            graphics_fill_rect(x + 7, y + 17, 22, 3, white);
            graphics_fill_rect(x + 7, y + 24, 22, 3, white);
            graphics_fill_rect(x + 12, y + 8, 4, 7, color);
            graphics_fill_rect(x + 21, y + 15, 4, 7, color);
            graphics_fill_rect(x + 14, y + 22, 4, 7, color);
            break;
        case VOS_APP_SETTINGS:
            graphics_fill_rect(x + 7, y + 10, 22, 3, white);
            graphics_fill_rect(x + 7, y + 17, 22, 3, white);
            graphics_fill_rect(x + 7, y + 24, 22, 3, white);
            graphics_fill_rect(x + 12, y + 8, 4, 7, white);
            graphics_fill_rect(x + 21, y + 15, 4, 7, white);
            graphics_fill_rect(x + 15, y + 22, 4, 7, white);
            break;
        case VOS_APP_HELP:
            graphics_draw_char('?', x + 13, y + 11, 2, white);
            break;
        case VOS_APP_FILES:
            graphics_fill_rect(x + 5, y + 11, 26, 19, white);
            graphics_fill_rect(x + 7, y + 14, 22, 13, color);
            graphics_fill_rect(x + 7, y + 8, 11, 5, white);
            graphics_fill_rect(x + 9, y + 10, 9, 3, color);
            graphics_fill_rect(x + 7, y + 17, 22, 2, white);
            break;
        case VOS_APP_STORE:
            graphics_fill_rect(x + 7, y + 12, 22, 19, white);
            graphics_fill_rect(x + 10, y + 15, 16, 13, color);
            graphics_fill_rect(x + 12, y + 7, 12, 3, white);
            graphics_fill_rect(x + 10, y + 10, 3, 5, white);
            graphics_fill_rect(x + 23, y + 10, 3, 5, white);
            graphics_fill_rect(x + 13, y + 19, 10, 2, white);
            break;
        case VOS_APP_CALCULATOR:
            graphics_fill_round_rect(x + 7, y + 5, 22, 27, 5, white);
            graphics_fill_rect(x + 10, y + 8, 16, 5, color);
            graphics_fill_rect(x + 10, y + 16, 4, 4, color);
            graphics_fill_rect(x + 17, y + 16, 4, 4, color);
            graphics_fill_rect(x + 24, y + 16, 3, 4, color);
            graphics_fill_rect(x + 10, y + 23, 4, 4, color);
            graphics_fill_rect(x + 17, y + 23, 4, 4, color);
            graphics_fill_rect(x + 24, y + 23, 3, 4, color);
            break;
        default:
            graphics_draw_char('?', x + 13, y + 11, 2, white);
            break;
    }
}

static void draw_dock(const struct desktop_state *state,
                      const struct desktop_palette *palette,
                      uint32_t text_scale) {
    uint64_t x = dock_x();
    uint64_t y = dock_y();
    uint64_t spacing = dock_spacing();
    uint64_t padding = (spacing - 36) / 2;
    uint64_t width = 8 + (uint64_t)VOS_APP_COUNT * spacing;
    struct graphics_color glass = blend_color(palette->dock, palette->white,
                                               state->dark_mode ? 30 : 54);
    struct graphics_color glass_edge = blend_color(glass, palette->white, 42);
    graphics_fill_round_rect(x + 8, y + 5, width - 16, 49, 15,
                             palette->shadow);
    graphics_fill_round_rect(x, y, width, 48, 15, glass);
    graphics_fill_round_rect(x + 3, y + 2, width - 6, 1, 1, glass_edge);
    graphics_fill_round_rect(x + 4, y + 45, width - 8, 1, 1,
                             blend_color(glass, palette->shadow, 72));
    graphics_fill_round_rect(x + 8, y, width - 16, 2, 1, palette->accent);

    for (uint32_t i = 0; i < VOS_APP_COUNT; i++) {
        uint64_t icon_x = x + padding + (uint64_t)i * spacing;
        uint64_t base_icon_y = y + 8;
        int hovered = point_in_rect(state->mouse_x, state->mouse_y,
                                    icon_x, base_icon_y, 36, 36);
        uint64_t icon_y = base_icon_y - (hovered ? 3 : 0);
        struct graphics_color icon_color = i == state->active_app
            ? palette->accent : dock_colors[i];
        if (hovered) {
            graphics_fill_round_rect(icon_x >= 2 ? icon_x - 2 : 0,
                                     icon_y >= 2 ? icon_y - 2 : 0, 40, 40, 9,
                                     (struct graphics_color){83, 101, 128});
        }
        draw_dock_icon(i, icon_x, icon_y, icon_color);
        if (i == state->active_app) {
            graphics_fill_rect(icon_x + 16, y + 47, 4, 2, palette->white);
        }
        if (hovered) {
            const char *label = locale_translate(dock_names[i]);
            uint64_t label_width = 6 * text_scale * text_length(label);
            uint64_t center = icon_x + 18;
            uint64_t label_x = center > label_width / 2
                ? center - label_width / 2 : 0;
            graphics_draw_text(label, label_x, y - 15, text_scale,
                               palette->white);
        }
    }
}

static void draw_wallpaper(const struct desktop_palette *palette) {
    uint64_t height = graphics_height();
    uint64_t top = 28;
    uint64_t content_height = height > top ? height - top : 0;
    const uint64_t bands = 96;
    uint64_t band_height = content_height / bands;
    uint64_t band_remainder = content_height % bands;
    for (uint64_t band = 0; band < bands; band++) {
        uint64_t y = top + band_height * band + band_remainder * band / bands;
        uint64_t next_band = band + 1;
        uint64_t next_y = top + band_height * next_band +
            band_remainder * next_band / bands;
        int32_t step = (int32_t)band;
        struct graphics_color color = {
            (uint8_t)((int32_t)palette->sky_top.red +
                ((int32_t)palette->sky_bottom.red - palette->sky_top.red) *
                step / (int32_t)bands),
            (uint8_t)((int32_t)palette->sky_top.green +
                ((int32_t)palette->sky_bottom.green - palette->sky_top.green) *
                step / (int32_t)bands),
            (uint8_t)((int32_t)palette->sky_top.blue +
                ((int32_t)palette->sky_bottom.blue - palette->sky_top.blue) *
                step / (int32_t)bands)
        };
        graphics_fill_rect(0, y, graphics_width(), next_y - y, color);
    }
    draw_wallpaper_glow(graphics_width() * 3 / 4, height * 52 / 100,
                        graphics_width() * 2 / 5, height * 42 / 100,
                        92, palette);
    draw_wallpaper_glow(graphics_width() / 5, height * 30 / 100,
                        graphics_width() / 4, height / 3, 55, palette);
    draw_wallpaper_glow(graphics_width() / 2, height * 88 / 100,
                        graphics_width() * 3 / 7, height / 5, 35, palette);
}

static void language_setup_bounds(uint64_t *x, uint64_t *y,
                                  uint64_t *width, uint64_t *height) {
    uint64_t screen_width = graphics_width();
    uint64_t screen_height = graphics_height();
    *width = screen_width > 544 ? 520
        : (screen_width > 24 ? screen_width - 24 : screen_width);
    *height = screen_height > 304 ? 280
        : (screen_height > 24 ? screen_height - 24 : screen_height);
    *x = (screen_width - *width) / 2;
    *y = (screen_height - *height) / 2;
}

static void draw_language_setup(const struct desktop_state *state,
                                const struct desktop_palette *palette,
                                uint32_t text_scale) {
    uint64_t x, y, width, height;
    language_setup_bounds(&x, &y, &width, &height);
    graphics_fill_round_rect(x + 5, y + 7, width, height, 18, palette->shadow);
    graphics_fill_round_rect(x, y, width, height, 18, palette->window);
    graphics_fill_round_rect(x, y, width, 38, 18, palette->titlebar);
    graphics_fill_rect(x, y + 18, width, 20, palette->titlebar);
    graphics_draw_text(locale_translate("BIENVENIDO A VOS"),
                       x + 22, y + 14, text_scale, palette->text);
    graphics_draw_text(locale_translate(state->setup_step == 0
                           ? "IDIOMA" : "TECLADO"),
                       x + 22, y + 54, text_scale, palette->accent);

    for (uint8_t option = 0; option < 2; option++) {
        uint64_t option_y = y + 82 + (uint64_t)option * 48;
        struct graphics_color option_color = option == state->setup_selection
            ? palette->accent : palette->card;
        struct graphics_color option_text = option == state->setup_selection
            ? palette->white : palette->text;
        graphics_fill_round_rect(x + 20, option_y, width - 40, 38, 10,
                                 option_color);
        const char *option_label = state->setup_step == 0
            ? (option == VOS_LANGUAGE_SPANISH ? "Español" : "English")
            : (option == 0 ? "Español (QWERTY)" : "US (QWERTY)");
        graphics_draw_text(locale_translate(option_label),
                           x + 34, option_y + 13, text_scale, option_text);
        graphics_draw_text(option == state->setup_selection ? ">" : " ",
                           x + width - 32, option_y + 13, text_scale,
                           option_text);
    }

    uint64_t button_width = width >= 190 ? 160 : width - 30;
    uint64_t button_x = x + (width - button_width) / 2;
    uint64_t button_y = y + height - 54;
    graphics_fill_round_rect(button_x, button_y, button_width, 36, 10,
                             palette->accent);
    const char *button_label = state->setup_step == 0
        ? "CONTINUAR" : "EMPEZAR";
    uint64_t label_width = text_length(locale_translate(button_label)) *
                           6 * text_scale;
    uint64_t label_x = button_x + (button_width - label_width) / 2;
    graphics_draw_text(locale_translate(button_label), label_x, button_y + 12,
                       text_scale, palette->white);
}

static int language_setup_option_at(int32_t mouse_x, int32_t mouse_y,
                                    uint8_t *selection) {
    uint64_t x, y, width, height;
    language_setup_bounds(&x, &y, &width, &height);
    for (uint8_t option = 0; option < 2; option++) {
        uint64_t option_y = y + 82 + (uint64_t)option * 48;
        if (point_in_rect(mouse_x, mouse_y, x + 20, option_y,
                          width - 40, 38)) {
            *selection = option;
            return 1;
        }
    }
    return 0;
}

static int language_setup_continue_at(int32_t mouse_x, int32_t mouse_y) {
    uint64_t x, y, width, height;
    language_setup_bounds(&x, &y, &width, &height);
    uint64_t button_width = width >= 190 ? 160 : width - 30;
    uint64_t button_x = x + (width - button_width) / 2;
    uint64_t button_y = y + height - 54;
    return point_in_rect(mouse_x, mouse_y, button_x, button_y,
                         button_width, 36);
}

static void advance_setup(struct desktop_state *state) {
    if (!locale_set((enum vos_language)state->setup_selection)) return;
    enum keyboard_layout layout = state->setup_selection == VOS_LANGUAGE_ENGLISH
        ? KEYBOARD_LAYOUT_US : KEYBOARD_LAYOUT_SPANISH;
    keyboard_set_layout(layout);
    state->setup_step = 1;
    state->setup_selection = layout == KEYBOARD_LAYOUT_SPANISH ? 0 : 1;
}

static void finish_setup(struct desktop_state *state) {
    keyboard_set_layout(state->setup_selection == 0
        ? KEYBOARD_LAYOUT_SPANISH : KEYBOARD_LAYOUT_US);
    state->setup_active = 0;
    state->active_app = VOS_APP_HOME;
}

static void draw_window(const struct desktop_state *state,
                        const struct limine_framebuffer *framebuffer,
                        int ps2_available, int mouse_available,
                        int memory_available, int memory_limited,
                        uint64_t free_pages, uint64_t total_pages,
                        const struct desktop_palette *palette) {
    uint64_t x = state->window_x;
    uint64_t y = state->window_y;
    uint64_t width = state->window_width;
    uint64_t height = state->window_height;
    struct app_theme app_theme = {
        .text = palette->text,
        .muted = palette->muted,
        .accent = palette->accent,
        .success = palette->success,
        .pending = palette->pending,
        .card = palette->card
    };

    graphics_fill_round_rect(x + 5, y + 7, width, height, 14, palette->shadow);
    graphics_fill_round_rect(x, y, width, height, 14, palette->window);
    graphics_fill_round_rect(x, y, width, 30, 14, palette->titlebar);
    graphics_fill_rect(x, y + 14, width, 16, palette->titlebar);
    draw_window_control(x + 11, y + 10, (struct graphics_color){218, 91, 84});
    draw_window_control(x + 27, y + 10, (struct graphics_color){224, 174, 69});
    draw_window_control(x + 43, y + 10, (struct graphics_color){88, 174, 106});
    uint32_t text_scale = graphics_width() >= 640 ? 2 : 1;
    graphics_fill_round_rect(x + 66, y + 8, 16, 16, 5,
                             dock_colors[state->active_app]);
    graphics_draw_char(apps_title(state->active_app)[0], x + 70, y + 12,
                       1, palette->white);
    graphics_draw_text(apps_title(state->active_app), x + 90, y + 8, text_scale,
                       palette->text);

    if (height > 44 && width > 36) {
        uint64_t content_width = width - 36;
        uint64_t content_height = height - 48;
        apps_draw_content(state->active_app, x + 18, y + 42,
                          content_width, content_height, framebuffer,
                          text_scale,
                          ps2_available, mouse_available,
                          memory_available, memory_limited, free_pages,
                          total_pages, state->dark_mode, state->accent_mode,
                          &app_theme);
    }
}

static void draw_desktop(const struct desktop_state *state,
                         const struct limine_framebuffer *framebuffer) {
    struct desktop_palette selected_palette = state->dark_mode
        ? dark_palette : light_palette;
    if (state->accent_mode) {
        selected_palette.accent = state->dark_mode
            ? (struct graphics_color){176, 137, 255}
            : (struct graphics_color){135, 92, 220};
        selected_palette.sky_top = state->dark_mode
            ? (struct graphics_color){49, 38, 76}
            : (struct graphics_color){105, 100, 173};
        selected_palette.sky_bottom = state->dark_mode
            ? (struct graphics_color){20, 18, 37}
            : (struct graphics_color){63, 60, 123};
    }
    const struct desktop_palette *palette = &selected_palette;
    uint32_t text_scale = graphics_width() >= 640 ? 2 : 1;
    draw_wallpaper(palette);
    graphics_fill_rect(0, 0, graphics_width(), 28, palette->bar);
    graphics_fill_rect(0, 27, graphics_width(), 1,
                       blend_color(palette->bar, palette->white, 26));
    graphics_fill_round_rect(12, 7, 12, 12, 4, palette->accent);
    graphics_fill_rect(16, 9, 4, 8, palette->white);
    graphics_draw_text("VOS", 31, 7, text_scale, palette->white);
    graphics_draw_text(apps_title(state->active_app), 72, 7, text_scale,
                       palette->muted);

    const char *status = locale_translate("SIN RED");
    uint64_t status_width = text_length(status) * 6 * text_scale;
    uint64_t status_x = graphics_width() > status_width + 30
        ? graphics_width() - status_width - 24 : 4;
    graphics_fill_round_rect(status_x - (status_x >= 8 ? 8 : 0), 4,
                             status_width + 16, 20, 8, palette->card);
    graphics_draw_text(status, status_x, 7, text_scale, palette->white);

    if (state->setup_active) {
        draw_dock(state, palette, text_scale);
        draw_language_setup(state, palette, text_scale);
        if (input_mouse_available()) {
            draw_cursor(state->mouse_x, state->mouse_y, palette);
        }
        return;
    }

    if (state->window_visible) {
        draw_window(state, framebuffer, input_controller_available(),
                    input_mouse_available(), pmm_is_ready(), pmm_is_limited(),
                    pmm_free_pages(), pmm_total_pages(), palette);
    }
    draw_dock(state, palette, text_scale);
    if (input_mouse_available()) {
        draw_cursor(state->mouse_x, state->mouse_y, palette);
    }
}

static void open_app(struct desktop_state *state, enum vos_app_id app) {
    if ((uint32_t)app >= (uint32_t)VOS_APP_COUNT) return;
    state->active_app = app;
    state->window_visible = 1;
}

static void move_window(struct desktop_state *state, int32_t mouse_x,
                        int32_t mouse_y) {
    int64_t x = (int64_t)mouse_x - state->drag_offset_x;
    int64_t y = (int64_t)mouse_y - state->drag_offset_y;
    uint64_t max_x = graphics_width() - state->window_width;
    uint64_t max_y = graphics_height() > state->window_height + 34
        ? graphics_height() - state->window_height - 34
        : (graphics_height() > state->window_height
            ? graphics_height() - state->window_height : 0);
    if (x < 0) x = 0;
    if (y < 30) y = 30;
    if ((uint64_t)x > max_x) x = (int64_t)max_x;
    if ((uint64_t)y > max_y) y = (int64_t)max_y;
    state->window_x = (uint64_t)x;
    state->window_y = (uint64_t)y;
}

static int dock_app_at(int32_t mouse_x, int32_t mouse_y,
                       enum vos_app_id *app) {
    uint64_t x = dock_x();
    uint64_t y = dock_y();
    for (uint32_t i = 0; i < VOS_APP_COUNT; i++) {
        uint64_t icon_x = x + (dock_spacing() - 36) / 2 +
                          (uint64_t)i * dock_spacing();
        if (point_in_rect(mouse_x, mouse_y, icon_x, y + 8, 36, 36)) {
            *app = (enum vos_app_id)i;
            return 1;
        }
    }
    return 0;
}

static int home_card_at(const struct desktop_state *state, int32_t mouse_x,
                        int32_t mouse_y, enum vos_app_id *app) {
    uint64_t content_width = state->window_width - 36;
    uint64_t content_height = state->window_height - 48;
    if (content_height < 270 || content_width < 150) return 0;
    uint64_t card_gap = 10;
    uint64_t card_width = (content_width - card_gap) / 2;
    uint64_t first_x = state->window_x + 18;
    uint64_t first_y = state->window_y + 42 + 38;
    for (uint32_t i = 0; i < VOS_APP_COUNT - 1; i++) {
        uint64_t x = first_x + (i % 2) * (card_width + card_gap);
        uint64_t y = first_y + (i / 2) * 58;
        if (point_in_rect(mouse_x, mouse_y, x, y, card_width, 52)) {
            *app = (enum vos_app_id)(i + 1);
            return 1;
        }
    }
    return 0;
}

static int settings_button_at(const struct desktop_state *state,
                              int32_t mouse_x, int32_t mouse_y) {
    uint64_t content_width = state->window_width - 36;
    uint64_t content_height = state->window_height - 48;
    if (content_height < 105) return 0;
    uint64_t button_width = content_width < 190 ? content_width : 190;
    return point_in_rect(mouse_x, mouse_y, state->window_x + 18,
                         state->window_y + 42 + 70, button_width, 32);
}

static int settings_accent_button_at(const struct desktop_state *state,
                                     int32_t mouse_x, int32_t mouse_y) {
    uint64_t content_width = state->window_width - 36;
    uint64_t content_height = state->window_height - 48;
    if (content_height < 175) return 0;
    uint64_t button_width = content_width < 190 ? content_width : 190;
    return point_in_rect(mouse_x, mouse_y, state->window_x + 18,
                         state->window_y + 42 + 140, button_width, 32);
}

static int settings_language_button_at(const struct desktop_state *state,
                                       int32_t mouse_x, int32_t mouse_y) {
    uint64_t content_width = state->window_width - 36;
    uint64_t content_height = state->window_height - 48;
    if (content_height < 270) return 0;
    uint64_t button_width = content_width < 190 ? content_width : 190;
    return point_in_rect(mouse_x, mouse_y, state->window_x + 18,
                         state->window_y + 42 + 230, button_width, 32);
}

static int settings_keyboard_button_at(const struct desktop_state *state,
                                       int32_t mouse_x, int32_t mouse_y) {
    uint64_t content_width = state->window_width - 36;
    uint64_t content_height = state->window_height - 48;
    if (content_height < 335) return 0;
    uint64_t button_width = content_width < 190 ? content_width : 190;
    return point_in_rect(mouse_x, mouse_y, state->window_x + 18,
                         state->window_y + 42 + 293, button_width, 32);
}

static void handle_mouse_down(struct desktop_state *state) {
    enum vos_app_id selected_app;
    if (state->setup_active) {
        if (language_setup_option_at(state->mouse_x, state->mouse_y,
                                     &state->setup_selection)) {
            return;
        }
        if (language_setup_continue_at(state->mouse_x, state->mouse_y)) {
            if (state->setup_step == 0) advance_setup(state);
            else finish_setup(state);
        }
        return;
    }
    if (state->mouse_y >= 0 && state->mouse_y < 28) {
        if (state->mouse_x >= 0 && state->mouse_x < 58) {
            open_app(state, VOS_APP_HOME);
        } else {
            uint32_t scale = graphics_width() >= 640 ? 2 : 1;
            uint64_t status_width = 7 * 6 * scale;
            uint64_t status_x = graphics_width() > status_width + 30
                ? graphics_width() - status_width - 24 : 4;
            if (state->mouse_x >= 0 &&
                (uint64_t)state->mouse_x >= status_x - (status_x >= 8 ? 8 : 0)) {
                open_app(state, VOS_APP_SERVICES);
            }
        }
        return;
    }
    if (dock_app_at(state->mouse_x, state->mouse_y, &selected_app)) {
        open_app(state, selected_app);
        return;
    }

    if (!state->window_visible) return;
    if (state->active_app == VOS_APP_SETTINGS &&
        settings_button_at(state, state->mouse_x, state->mouse_y)) {
        state->dark_mode = !state->dark_mode;
        return;
    }
    if (state->active_app == VOS_APP_SETTINGS &&
        settings_accent_button_at(state, state->mouse_x, state->mouse_y)) {
        state->accent_mode = !state->accent_mode;
        return;
    }
    if (state->active_app == VOS_APP_SETTINGS &&
        settings_language_button_at(state, state->mouse_x, state->mouse_y)) {
        locale_toggle();
        return;
    }
    if (state->active_app == VOS_APP_SETTINGS &&
        settings_keyboard_button_at(state, state->mouse_x, state->mouse_y)) {
        keyboard_toggle_layout();
        return;
    }
    if (state->active_app == VOS_APP_FILES) {
        uint64_t content_x = state->window_x + 18;
        uint64_t content_y = state->window_y + 42;
        uint64_t content_width = state->window_width - 36;
        uint64_t content_height = state->window_height - 48;
        if (point_in_rect(state->mouse_x, state->mouse_y, content_x,
                          content_y, content_width, content_height)) {
            apps_handle_file_click((uint64_t)state->mouse_x - content_x,
                                   (uint64_t)state->mouse_y - content_y,
                                   content_width, content_height);
            return;
        }
    }
    if (state->active_app == VOS_APP_CALCULATOR) {
        uint64_t content_x = state->window_x + 18;
        uint64_t content_y = state->window_y + 42;
        apps_handle_calculator_click((uint64_t)state->mouse_x - content_x,
                                     (uint64_t)state->mouse_y - content_y,
                                     state->window_width - 36,
                                     state->window_height - 48);
        return;
    }
    if (state->active_app == VOS_APP_STORE) {
        uint64_t content_x = state->window_x + 18;
        uint64_t content_y = state->window_y + 42;
        uint64_t content_width = state->window_width - 36;
        uint64_t content_height = state->window_height - 48;
        if (point_in_rect(state->mouse_x, state->mouse_y, content_x,
                          content_y, content_width, content_height)) {
            int app = apps_store_app_at((uint64_t)state->mouse_x - content_x,
                                        (uint64_t)state->mouse_y - content_y,
                                        content_width, content_height);
            if (app >= 0) open_app(state, (enum vos_app_id)app);
            return;
        }
    }
    if (state->active_app == VOS_APP_HOME &&
        home_card_at(state, state->mouse_x, state->mouse_y, &selected_app)) {
        open_app(state, selected_app);
        return;
    }

    uint64_t x = state->window_x;
    uint64_t y = state->window_y;
    if (point_in_rect(state->mouse_x, state->mouse_y, x + 8, y + 6, 16, 18)) {
        if (state->active_app == VOS_APP_HOME) {
            state->window_visible = 0;
        } else {
            open_app(state, VOS_APP_HOME);
        }
    } else if (point_in_rect(state->mouse_x, state->mouse_y,
                             x + 24, y + 6, 16, 18)) {
        state->window_visible = 0;
    } else if (point_in_rect(state->mouse_x, state->mouse_y,
                             x + 40, y + 6, 16, 18)) {
        if (state->maximized) {
            state->window_x = state->restore_x;
            state->window_y = state->restore_y;
            state->window_width = state->restore_width;
            state->window_height = state->restore_height;
            state->maximized = 0;
        } else {
            state->restore_x = state->window_x;
            state->restore_y = state->window_y;
            state->restore_width = state->window_width;
            state->restore_height = state->window_height;
            state->window_x = graphics_width() > 20 ? 10 : 0;
            state->window_y = graphics_height() > 96 ? 32
                : (graphics_height() > 34 ? 30 : 0);
            state->window_width = graphics_width() > 20
                ? graphics_width() - 20 : graphics_width();
            state->window_height = graphics_height() > 96
                ? graphics_height() - 96
                : (graphics_height() > 34
                    ? graphics_height() - 34 : graphics_height());
            state->maximized = 1;
        }
    } else if (point_in_rect(state->mouse_x, state->mouse_y, x, y,
                             state->window_width, 30) && !state->maximized) {
        state->dragging = 1;
        state->drag_offset_x = state->mouse_x - (int32_t)x;
        state->drag_offset_y = state->mouse_y - (int32_t)y;
    }
}

void desktop_main(const struct limine_framebuffer *framebuffer) {
    uint64_t screen_width = graphics_width();
    uint64_t screen_height = graphics_height();
    struct desktop_state state = {0};
    locale_init();
    apps_init();
    state.window_width = screen_width > 692 ? 660
        : (screen_width > 32 ? screen_width - 32 : screen_width);
    uint64_t usable_height = screen_height > 34 ? screen_height - 34
                                                : screen_height;
    state.window_height = screen_height > 520 ? 420
        : (screen_height > 100 ? screen_height - 100 : usable_height);
    if (state.window_height > usable_height) state.window_height = usable_height;
    if (state.window_height < 80 && usable_height >= 80) state.window_height = 80;
    state.window_x = (screen_width - state.window_width) / 2;
    state.window_y = screen_height > state.window_height + 88
        ? 28 + (screen_height - state.window_height - 88) / 2
        : (screen_height > 32 ? 30 : 0);
    state.restore_x = state.window_x;
    state.restore_y = state.window_y;
    state.restore_width = state.window_width;
    state.restore_height = state.window_height;
    state.mouse_x = (int32_t)(screen_width / 2);
    state.mouse_y = (int32_t)(screen_height / 2);
    state.active_app = VOS_APP_HOME;
    state.window_visible = 1;
    state.setup_active = 1;
    state.setup_selection = VOS_LANGUAGE_SPANISH;

    int redraw = 1;
    for (;;) {
        if (redraw) {
            draw_desktop(&state, framebuffer);
            redraw = 0;
        }
        struct input_event event;
        if (!input_poll_event(&event)) {
            __asm__ volatile ("pause");
            continue;
        }
        if (event.type == INPUT_EVENT_MOUSE) {
            redraw = event.mouse_x != state.mouse_x ||
                     event.mouse_y != state.mouse_y ||
                     event.mouse_buttons_changed != 0;
            state.mouse_x = event.mouse_x;
            state.mouse_y = event.mouse_y;
            if (state.dragging && (event.mouse_buttons & INPUT_MOUSE_LEFT)) {
                move_window(&state, state.mouse_x, state.mouse_y);
            }
            if ((event.mouse_buttons_changed & INPUT_MOUSE_LEFT) != 0) {
                if ((event.mouse_buttons & INPUT_MOUSE_LEFT) == 0) {
                    state.dragging = 0;
                } else {
                    handle_mouse_down(&state);
                }
            }
        } else if (event.type == INPUT_EVENT_KEY && event.pressed) {
            redraw = 1;
            if (state.setup_active) {
                if (event.key == INPUT_KEY_UP) {
                    state.setup_selection = 0;
                } else if (event.key == INPUT_KEY_DOWN) {
                    state.setup_selection = 1;
                } else if (event.character == '1') {
                    state.setup_selection = 0;
                    if (state.setup_step == 0) advance_setup(&state);
                    else finish_setup(&state);
                } else if (event.character == '2') {
                    state.setup_selection = 1;
                    if (state.setup_step == 0) advance_setup(&state);
                    else finish_setup(&state);
                } else if (event.key == INPUT_KEY_ENTER) {
                    if (state.setup_step == 0) advance_setup(&state);
                    else finish_setup(&state);
                }
            } else if (apps_handle_key(state.active_app, event.key,
                                event.character)) {
                redraw = 1;
            } else if (state.active_app == VOS_APP_SETTINGS &&
                (event.key == INPUT_KEY_ENTER || event.character == ' ')) {
                state.dark_mode = !state.dark_mode;
            } else if (state.active_app == VOS_APP_SETTINGS &&
                       (event.character == 'c' || event.character == 'C')) {
                state.accent_mode = !state.accent_mode;
            } else if (state.active_app == VOS_APP_SETTINGS &&
                       (event.character == 'l' || event.character == 'L')) {
                locale_toggle();
            } else if (state.active_app == VOS_APP_SETTINGS &&
                       (event.character == 'k' || event.character == 'K')) {
                keyboard_toggle_layout();
            } else if (event.key == INPUT_KEY_ESCAPE) {
                if (state.active_app != VOS_APP_HOME) {
                    open_app(&state, VOS_APP_HOME);
                } else {
                    state.window_visible = !state.window_visible;
                }
            } else if (event.key == INPUT_KEY_TAB) {
                open_app(&state, (enum vos_app_id)((state.active_app + 1) %
                                                   VOS_APP_COUNT));
            } else if (event.character >= '1' && event.character <= '9') {
                open_app(&state, (enum vos_app_id)(event.character - '1'));
            } else if (event.key == 0x03b) {
                open_app(&state, VOS_APP_HELP);
            } else if (event.character == 'h' || event.character == 'H') {
                open_app(&state, VOS_APP_HOME);
            } else if (event.character == 's' || event.character == 'S') {
                open_app(&state, VOS_APP_SYSTEM);
            } else if (event.character == 'm' || event.character == 'M') {
                open_app(&state, VOS_APP_MEMORY);
            } else if (event.character == 'a' || event.character == 'A') {
                open_app(&state, VOS_APP_SETTINGS);
            } else if (event.character == 'f' || event.character == 'F') {
                open_app(&state, VOS_APP_FILES);
            } else if (event.character == 't' || event.character == 'T') {
                open_app(&state, VOS_APP_STORE);
            }
        }
    }
}

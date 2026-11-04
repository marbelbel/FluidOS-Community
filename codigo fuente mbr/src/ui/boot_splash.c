#include "boot_splash.h"
#include "graphics.h"

static uint64_t splash_bar_x;
static uint64_t splash_bar_y;
static uint64_t splash_bar_width;
static uint8_t splash_layout_ready;

static void draw_splash_layout(void) {
    uint64_t width = graphics_width();
    uint64_t height = graphics_height();
    if (width == 0 || height == 0) return;

    graphics_clear((struct graphics_color){0, 0, 0});

    uint64_t center_x = width / 2;
    uint64_t center_y = height / 2;
    uint64_t tile = width < 420 ? 56 : 72;
    if (width < 240 && tile > width / 3) tile = width / 3;
    if (height < 240 && tile > height / 3) tile = height / 3;
    if (tile < 14) return;

    uint64_t tile_x = center_x - tile / 2;
    uint64_t group_half_height = (tile + 36) / 2;
    uint64_t tile_y = center_y > group_half_height
        ? center_y - group_half_height : 4;
    graphics_fill_round_rect(tile_x + 2, tile_y + 4, tile, tile, 18,
                             (struct graphics_color){16, 18, 29});
    graphics_fill_round_rect(tile_x, tile_y, tile, tile, 18,
                             (struct graphics_color){117, 83, 232});
    graphics_fill_round_rect(tile_x + 5, tile_y + 5, tile - 10, tile - 10, 14,
                             (struct graphics_color){61, 139, 240});

    uint32_t logo_scale = tile >= 72 ? 8 : (tile >= 56 ? 6 : (uint32_t)(tile / 7));
    if (logo_scale == 0) logo_scale = 1;
    graphics_draw_char('V', center_x - (5 * logo_scale) / 2,
                       tile_y + (tile - 7 * logo_scale) / 2,
                       logo_scale, (struct graphics_color){255, 255, 255});

    splash_bar_width = width > 80 ? width - 80 : width;
    if (splash_bar_width > 300) splash_bar_width = 300;
    splash_bar_x = (width - splash_bar_width) / 2;
    splash_bar_y = tile_y + tile + 28;
    if (splash_bar_y + 9 > height) {
        splash_bar_y = height > 12 ? height - 12 : 0;
    }

    graphics_fill_round_rect(splash_bar_x, splash_bar_y, splash_bar_width, 8, 4,
                             (struct graphics_color){42, 44, 54});
    splash_layout_ready = 1;
}

void boot_splash_draw(uint8_t phase) {
    static const uint8_t progress[] = {8, 34, 68, 92};
    if (phase > 3) phase = 3;
    if (!splash_layout_ready) draw_splash_layout();
    if (!splash_layout_ready || splash_bar_width == 0) return;

    graphics_fill_round_rect(splash_bar_x, splash_bar_y, splash_bar_width, 8, 4,
                             (struct graphics_color){42, 44, 54});
    uint64_t fill = splash_bar_width * progress[phase] / 100;
    if (fill != 0) {
        graphics_fill_round_rect(splash_bar_x, splash_bar_y, fill, 8, 4,
                                 (struct graphics_color){111, 174, 255});
    }
}

void boot_splash_error(void) {
    if (!splash_layout_ready) draw_splash_layout();
}

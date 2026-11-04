#ifndef VOS_APPS_H
#define VOS_APPS_H

#include "graphics.h"
#include "limine.h"

enum vos_app_id {
    VOS_APP_HOME = 0,
    VOS_APP_SYSTEM,
    VOS_APP_MEMORY,
    VOS_APP_SERVICES,
    VOS_APP_SETTINGS,
    VOS_APP_HELP,
    VOS_APP_FILES,
    VOS_APP_STORE,
    VOS_APP_CALCULATOR,
    VOS_APP_COUNT
};

struct app_theme {
    struct graphics_color text;
    struct graphics_color muted;
    struct graphics_color accent;
    struct graphics_color success;
    struct graphics_color pending;
    struct graphics_color card;
};

const char *apps_title(enum vos_app_id app);
void apps_init(void);
int apps_handle_key(enum vos_app_id app, uint16_t key, uint32_t character);
void apps_handle_file_click(uint64_t x, uint64_t y,
                            uint64_t width, uint64_t height);
void apps_handle_calculator_click(uint64_t x, uint64_t y,
                                  uint64_t width, uint64_t height);
int apps_store_app_at(uint64_t x, uint64_t y,
                      uint64_t width, uint64_t height);
void apps_draw_content(enum vos_app_id app, uint64_t x, uint64_t y,
                       uint64_t width, uint64_t height,
                       const struct limine_framebuffer *framebuffer,
                       uint32_t text_scale,
                       int ps2_available, int mouse_available,
                       int memory_available, int memory_limited,
                       uint64_t free_pages, uint64_t total_pages,
                       int dark_mode, int accent_mode,
                       const struct app_theme *theme);

#endif

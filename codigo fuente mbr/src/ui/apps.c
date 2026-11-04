#include "apps.h"
#include "input.h"
#include "keyboard.h"
#include "locale.h"
#include "ramfs.h"

static const char *const app_titles[VOS_APP_COUNT] = {
    "INICIO", "SISTEMA VOS", "MEMORIA", "CENTRO VOS", "AJUSTES", "AYUDA",
    "ARCHIVOS RAM", "APLICACIONES", "CALCULADORA"
};

struct store_entry {
    enum vos_app_id app;
    const char *name;
    char icon;
    struct graphics_color color;
};

static const struct store_entry store_catalog[] = {
    {VOS_APP_SYSTEM, "Sistema", 'S', {66, 126, 207}},
    {VOS_APP_MEMORY, "Memoria", 'M', {44, 150, 111}},
    {VOS_APP_SERVICES, "Centro VOS", 'V', {132, 91, 207}},
    {VOS_APP_SETTINGS, "Ajustes", 'A', {215, 145, 53}},
    {VOS_APP_HELP, "Ayuda", '?', {39, 149, 160}},
    {VOS_APP_FILES, "Archivos RAM", 'F', {63, 145, 126}},
    {VOS_APP_CALCULATOR, "Calculadora", 'C', {101, 116, 145}}
};

static uint32_t app_text_scale = 1;
static uint32_t selected_file;
static uint32_t editing_file;
static uint32_t file_scroll;
static uint32_t visible_file_rows;
static uint8_t files_editing;
static int64_t calculator_total;
static int64_t calculator_entry;
static char calculator_operator;
static uint8_t calculator_entry_active;
static uint8_t calculator_result_shown;
static uint8_t calculator_error;
#define app_text(text, x, y, scale, color) \
    graphics_draw_text(locale_translate(text), (x), (y), \
                       (scale) * app_text_scale, (color))

void apps_init(void) {
    ramfs_init();
    selected_file = 0;
    editing_file = 0;
    file_scroll = 0;
    visible_file_rows = 0;
    files_editing = 0;
    calculator_total = 0;
    calculator_entry = 0;
    calculator_operator = 0;
    calculator_entry_active = 0;
    calculator_result_shown = 0;
    calculator_error = 0;
}

static void calculator_clear(void) {
    calculator_total = 0;
    calculator_entry = 0;
    calculator_operator = 0;
    calculator_entry_active = 0;
    calculator_result_shown = 0;
    calculator_error = 0;
}

static int calculator_apply(char operation, int64_t left, int64_t right,
                            int64_t *result) {
    if (result == 0) return 0;
    switch (operation) {
        case '+':
            if ((right > 0 && left > INT64_MAX - right) ||
                (right < 0 && left < INT64_MIN - right)) return 0;
            *result = left + right;
            return 1;
        case '-':
            if ((right < 0 && left > INT64_MAX + right) ||
                (right > 0 && left < INT64_MIN + right)) return 0;
            *result = left - right;
            return 1;
        case '*':
            if ((left == -1 && right == INT64_MIN) ||
                (right == -1 && left == INT64_MIN)) return 0;
            if (left > 0) {
                if ((right > 0 && left > INT64_MAX / right) ||
                    (right < 0 && right < INT64_MIN / left)) return 0;
            } else if (left < 0) {
                if ((right > 0 && left < INT64_MIN / right) ||
                    (right < 0 && left < INT64_MAX / right)) return 0;
            }
            *result = left * right;
            return 1;
        case '/':
            if (right == 0 || (left == INT64_MIN && right == -1)) return 0;
            *result = left / right;
            return 1;
        default:
            return 0;
    }
}

static void calculator_append_digit(uint8_t digit) {
    if (digit > 9 || calculator_error) return;
    if (calculator_result_shown) {
        calculator_total = 0;
        calculator_entry = 0;
        calculator_operator = 0;
        calculator_entry_active = 0;
        calculator_result_shown = 0;
    }
    if (calculator_entry > (INT64_MAX - digit) / 10) {
        calculator_error = 1;
        return;
    }
    calculator_entry = calculator_entry * 10 + digit;
    calculator_entry_active = 1;
}

static void calculator_press_operator(char operation) {
    if (calculator_error) return;
    if (calculator_entry_active) {
        if (calculator_operator != 0) {
            int64_t result;
            if (!calculator_apply(calculator_operator, calculator_total,
                                 calculator_entry, &result)) {
                calculator_error = 1;
                return;
            }
            calculator_total = result;
        } else {
            calculator_total = calculator_entry;
        }
    } else if (calculator_operator == 0) {
        calculator_total = 0;
    }
    calculator_operator = operation;
    calculator_entry_active = 0;
    calculator_result_shown = 0;
}

static void calculator_equals(void) {
    if (calculator_error || calculator_operator == 0 ||
        !calculator_entry_active) return;
    int64_t result;
    if (!calculator_apply(calculator_operator, calculator_total,
                          calculator_entry, &result)) {
        calculator_error = 1;
        return;
    }
    calculator_total = result;
    calculator_entry = result;
    calculator_operator = 0;
    calculator_entry_active = 1;
    calculator_result_shown = 1;
}

static void calculator_backspace(void) {
    if (calculator_error) return;
    if (calculator_result_shown) {
        calculator_clear();
        return;
    }
    if (!calculator_entry_active) {
        calculator_entry = 0;
        calculator_entry_active = 1;
        return;
    }
    calculator_entry /= 10;
}

static void calculator_press(char key) {
    if (key >= '0' && key <= '9') {
        calculator_append_digit((uint8_t)(key - '0'));
    } else if (key == 'C') {
        calculator_clear();
    } else if (key == '<') {
        calculator_backspace();
    } else if (key == 'Z') {
        calculator_append_digit(0);
        calculator_append_digit(0);
    } else if (key == '+' || key == '-' || key == '*' || key == '/') {
        calculator_press_operator(key);
    } else if (key == '=') {
        calculator_equals();
    }
}

static void select_file(uint32_t index) {
    selected_file = index;
    if (visible_file_rows == 0) return;
    if (selected_file < file_scroll) file_scroll = selected_file;
    if (selected_file >= file_scroll + visible_file_rows) {
        file_scroll = selected_file - visible_file_rows + 1;
    }
}

int apps_handle_key(enum vos_app_id app, uint16_t key, uint32_t character) {
    if (app == VOS_APP_CALCULATOR) {
        if (key == INPUT_KEY_BACKSPACE) {
            calculator_backspace();
            return 1;
        }
        if (key == INPUT_KEY_ENTER || character == '\n') {
            calculator_equals();
            return 1;
        }
        if (character >= '0' && character <= '9') {
            calculator_press((char)character);
            return 1;
        }
        if (character == 'c' || character == 'C') {
            calculator_clear();
            return 1;
        }
        if (character == '+' || character == '-' || character == '*' ||
            character == '/' || character == '=') {
            calculator_press((char)character);
            return 1;
        }
        return 0;
    }

    if (app != VOS_APP_FILES) return 0;

    if (files_editing) {
        if (key == INPUT_KEY_ESCAPE) {
            files_editing = 0;
            select_file(editing_file);
            return 1;
        }
        if (key == INPUT_KEY_BACKSPACE) {
            (void)ramfs_backspace(editing_file);
            return 1;
        }
        if (key == INPUT_KEY_ENTER || character == '\n') {
            (void)ramfs_append(editing_file, '\n');
            return 1;
        }
        if (character >= 0x20 && character != 0x7f) {
            (void)ramfs_append(editing_file, character);
        }
        return 1;
    }

    uint32_t count = ramfs_count();
    if (key == INPUT_KEY_UP) {
        if (selected_file > 0) select_file(selected_file - 1);
        return 1;
    }
    if (key == INPUT_KEY_DOWN) {
        if (selected_file + 1 < count) select_file(selected_file + 1);
        return 1;
    }
    if (key == INPUT_KEY_ENTER && selected_file < count) {
        editing_file = selected_file;
        files_editing = 1;
        return 1;
    }
    if (character == 'n' || character == 'N') {
        int index = ramfs_create_note();
        if (index >= 0) {
            select_file((uint32_t)index);
            editing_file = (uint32_t)index;
            files_editing = 1;
        }
        return 1;
    }
    return 0;
}

void apps_handle_file_click(uint64_t x, uint64_t y,
                            uint64_t width, uint64_t height) {
    if (files_editing || x >= width || y < 42 * app_text_scale) return;
    uint64_t row_height = 22 * app_text_scale;
    uint64_t row = (y - 42 * app_text_scale) / row_height;
    uint64_t visible_rows = height > 72 * app_text_scale
        ? (height - 72 * app_text_scale) / row_height : 0;
    uint64_t count = ramfs_count();
    if (row < visible_rows && file_scroll + row < count) {
        select_file(file_scroll + (uint32_t)row);
    }
}

int apps_store_app_at(uint64_t x, uint64_t y,
                      uint64_t width, uint64_t height) {
    if (width < 160 || height < 270 || x >= width || y >= height) return -1;
    uint64_t gap = 10;
    uint64_t card_width = (width - gap) / 2;
    uint64_t card_y = 36;
    for (uint32_t i = 0; i < sizeof(store_catalog) / sizeof(store_catalog[0]); i++) {
        uint64_t card_x = (i % 2) * (card_width + gap);
        uint64_t top = card_y + (i / 2) * 70;
        if (x >= card_x && y >= top && x - card_x < card_width && y - top < 62) {
            return store_catalog[i].app;
        }
    }
    return -1;
}

const char *apps_title(enum vos_app_id app) {
    if ((uint32_t)app >= (uint32_t)VOS_APP_COUNT) {
        return "VOS";
    }
    return locale_translate(app_titles[app]);
}

static void draw_number(uint64_t value, uint64_t x, uint64_t y,
                        struct graphics_color color) {
    char digits[20];
    uint32_t count = 0;
    if (value == 0) {
        graphics_draw_char('0', x, y, app_text_scale, color);
        return;
    }
    while (value != 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (count != 0) {
        graphics_draw_char(digits[--count], x, y, app_text_scale, color);
        x += 6 * app_text_scale;
    }
}

static uint64_t calculator_magnitude(int64_t value) {
    return value < 0 ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
}

static uint32_t calculator_number_length(int64_t value) {
    uint64_t magnitude = calculator_magnitude(value);
    uint32_t length = value < 0 ? 1 : 0;
    do {
        length++;
        magnitude /= 10;
    } while (magnitude != 0);
    return length;
}

static void draw_calculator_number(int64_t value, uint64_t x, uint64_t y,
                                   uint32_t scale,
                                   struct graphics_color color) {
    uint64_t magnitude = calculator_magnitude(value);
    char digits[20];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0 && count < sizeof(digits));

    if (value < 0) {
        graphics_draw_char('-', x, y, scale, color);
        x += 6 * scale;
    }
    while (count != 0) {
        graphics_draw_char(digits[--count], x, y, scale, color);
        x += 6 * scale;
    }
}

static uint64_t calculator_button_height(uint64_t content_height) {
    if (content_height <= 124) return 0;
    uint64_t button_height = (content_height - 124) / 5;
    if (button_height > 44) button_height = 44;
    if (button_height < 24) return 0;
    return button_height;
}

static void draw_calculator_button(const char *label, uint64_t x, uint64_t y,
                                   uint64_t width, uint64_t height,
                                   int special,
                                   const struct app_theme *theme) {
    struct graphics_color fill = special == 1
        ? theme->accent : (special == 2
            ? (struct graphics_color){214, 91, 94} : theme->card);
    struct graphics_color text = special == 0 ? theme->text : theme->white;
    graphics_fill_round_rect(x, y, width, height, 9, fill);
    uint64_t label_width = text_length(label) * 6 * app_text_scale;
    uint64_t label_x = x + (width > label_width ? (width - label_width) / 2 : 0);
    uint64_t label_y = y + (height > 7 * app_text_scale
        ? (height - 7 * app_text_scale) / 2 : 0);
    graphics_draw_text(label, label_x, label_y, app_text_scale, text);
}

static void draw_calculator(uint64_t x, uint64_t y, uint64_t width,
                            uint64_t height, const struct app_theme *theme) {
    static const char *const labels[4][4] = {
        {"C", "<", "/", "*"},
        {"7", "8", "9", "-"},
        {"4", "5", "6", "+"},
        {"1", "2", "3", "="}
    };
    if (width < 160 || height < 190) return;

    graphics_fill_round_rect(x, y, width, 54, 10, theme->card);
    if (calculator_error) {
        app_text("ERROR", x + width - 76, y + 20, 1, theme->pending);
    } else {
        int64_t shown = calculator_entry_active
            ? calculator_entry : calculator_total;
        uint32_t scale = app_text_scale + 1;
        uint64_t number_width =
            (uint64_t)calculator_number_length(shown) * 6 * scale;
        uint64_t number_x = width > number_width + 16
            ? x + width - number_width - 16 : x + 8;
        draw_calculator_number(shown, number_x, y + 17, scale, theme->text);
    }

    uint64_t button_height = calculator_button_height(height);
    uint64_t gap = 8;
    uint64_t cell_width = width > gap * 3
        ? (width - gap * 3) / 4 : 0;
    uint64_t keypad_y = y + 66;
    if (button_height == 0 || cell_width == 0) return;

    for (uint32_t row = 0; row < 4; row++) {
        uint64_t button_y = keypad_y + (uint64_t)row * (button_height + gap);
        for (uint32_t column = 0; column < 4; column++) {
            const char *label = labels[row][column];
            int special = label[0] == 'C' ? 2
                : ((row == 0 || column == 3) ? 1 : 0);
            uint64_t button_x = x + (uint64_t)column * (cell_width + gap);
            draw_calculator_button(label, button_x, button_y, cell_width,
                                   button_height, special, theme);
        }
    }

    uint64_t zero_y = keypad_y + 4 * (button_height + gap);
    uint64_t zero_width = cell_width * 3 + gap * 2;
    draw_calculator_button("0", x, zero_y, zero_width,
                           button_height, 0, theme);
    draw_calculator_button("00", x + zero_width + gap, zero_y,
                           cell_width, button_height, 0, theme);
}

static char calculator_button_at(uint64_t x, uint64_t y,
                                 uint64_t width, uint64_t height) {
    static const char keys[4][4] = {
        {'C', '<', '/', '*'},
        {'7', '8', '9', '-'},
        {'4', '5', '6', '+'},
        {'1', '2', '3', '='}
    };
    uint64_t button_height = calculator_button_height(height);
    const uint64_t gap = 8;
    if (button_height == 0 || width <= gap * 3 || x >= width ||
        y >= height || y < 66) return 0;
    uint64_t cell_width = (width - gap * 3) / 4;
    uint64_t local_y = y - 66;
    uint64_t row_step = button_height + gap;
    uint64_t row = local_y / row_step;
    if (row > 4 || local_y % row_step >= button_height) return 0;
    if (row == 4) {
        uint64_t zero_width = cell_width * 3 + gap * 2;
        if (x < zero_width) return '0';
        if (x >= zero_width + gap && x < zero_width + gap + cell_width) {
            return 'Z';
        }
        return 0;
    }
    uint64_t column_step = cell_width + gap;
    uint64_t column = x / column_step;
    if (column > 3 || x % column_step >= cell_width) return 0;
    return keys[row][column];
}

void apps_handle_calculator_click(uint64_t x, uint64_t y,
                                  uint64_t width, uint64_t height) {
    char key = calculator_button_at(x, y, width, height);
    if (key != 0) calculator_press(key);
}

static void draw_home_card(const char *title, char icon,
                           struct graphics_color icon_color,
                           uint64_t x, uint64_t y, uint64_t width,
                           const struct app_theme *theme) {
    graphics_fill_round_rect(x, y, width, 52, 12, theme->card);
    graphics_fill_round_rect(x + 9, y + 9, 34, 34, 10, icon_color);
    graphics_draw_char(icon, x + 20, y + 18, 2,
                       (struct graphics_color){255, 255, 255});
    app_text(title, x + 53, y + 20, 1, theme->text);
    graphics_draw_char('>', x + width - 20, y + 21, 1, theme->muted);
}

static void draw_home(uint64_t x, uint64_t y, uint64_t width,
                      uint64_t height, const struct app_theme *theme) {
    static const char icons[] = {'S', 'M', 'V', 'A', '?', 'F', 'A', 'C'};
    static const struct graphics_color colors[] = {
        {66, 126, 207}, {44, 150, 111}, {132, 91, 207},
        {215, 145, 53}, {39, 149, 160}, {63, 145, 126},
        {216, 85, 111}, {101, 116, 145}
    };
    if (height < 270 || width < 150) return;

    app_text("APLICACIONES", x, y, 2, theme->text);
    uint64_t card_y = y + 38;
    uint64_t gap = 10;
    uint64_t card_width = (width - gap) / 2;
    for (uint32_t i = 0; i < sizeof(icons); i++) {
        uint64_t card_x = x + (i % 2) * (card_width + gap);
        uint64_t row_y = card_y + (i / 2) * 58;
        draw_home_card(apps_title((enum vos_app_id)(i + 1)), icons[i],
                       colors[i], card_x, row_y, card_width, theme);
    }
}

static void draw_store(uint64_t x, uint64_t y, uint64_t width,
                       uint64_t height, const struct app_theme *theme) {
    app_text("APLICACIONES", x, y, 2, theme->text);

    if (width < 160 || height < 270) {
        return;
    }

    uint64_t gap = 10;
    uint64_t card_width = (width - gap) / 2;
    for (uint32_t i = 0; i < sizeof(store_catalog) / sizeof(store_catalog[0]); i++) {
        const struct store_entry *entry = &store_catalog[i];
        uint64_t card_x = x + (i % 2) * (card_width + gap);
        uint64_t card_y = y + 36 + (i / 2) * 70;
        graphics_fill_round_rect(card_x, card_y, card_width, 62, 10,
                                 theme->card);
        graphics_fill_round_rect(card_x + 8, card_y + 15, 30, 30, 8,
                                 entry->color);
        graphics_draw_char(entry->icon, card_x + 18, card_y + 23,
                           app_text_scale, (struct graphics_color){255, 255, 255});
        app_text(entry->name, card_x + 46, card_y + 10, 1, theme->text);
        uint64_t button_x = card_width >= 190 ? card_x + card_width - 82
                                               : card_x + 8;
        graphics_fill_round_rect(button_x, card_y + 38, 74, 18, 7,
                                 theme->accent);
        struct graphics_color button_foreground = {255, 255, 255};
        app_text("ABRIR", button_x + 9, card_y + 49, 1,
                 button_foreground);
    }

}

static void draw_system(uint64_t x, uint64_t y, uint64_t width,
                        uint64_t height,
                        const struct limine_framebuffer *framebuffer,
                        int ps2_available, int mouse_available,
                        const struct app_theme *theme) {
    app_text("INFORMACIÓN DEL SISTEMA", x, y, 1, theme->accent);
    app_text("VOS 0.9   ARQUITECTURA X86-64", x, y + 22, 1,
                       theme->text);
    app_text("ARRANQUE: LIMINE", x, y + 42, 1, theme->text);
    if (height >= 128 && framebuffer != 0 && width >= 150) {
        app_text("PANTALLA:", x, y + 62, 1, theme->text);
        draw_number(framebuffer->width, x + 60 * app_text_scale, y + 62,
                    theme->text);
        app_text("X", x + 92 * app_text_scale, y + 62, 1, theme->text);
        draw_number(framebuffer->height, x + 104 * app_text_scale, y + 62,
                    theme->text);
        app_text("PIXEL:", x, y + 80, 1, theme->text);
        draw_number(framebuffer->bpp, x + 42 * app_text_scale, y + 80,
                    theme->text);
        app_text("BITS", x + 60 * app_text_scale, y + 80, 1, theme->text);
        app_text("CONTROLADOR PS/2:", x, y + 100, 1, theme->text);
        app_text(ps2_available ? "ACTIVO" : "NO DISPONIBLE",
                           x + 108 * app_text_scale, y + 100, 1,
                           ps2_available ? theme->success : theme->pending);
    }
    if (height >= 148) {
        app_text("RATON:", x, y + 120, 1, theme->text);
        app_text(mouse_available ? "DISPONIBLE" : "OPCIONAL AUSENTE",
                           x + 42 * app_text_scale, y + 120, 1,
                           mouse_available ? theme->success : theme->muted);
    }
    if (height >= 175) {
        app_text("VOS ESTÁ EN DESARROLLO.", x, y + 145,
                           1, theme->muted);
    }
}

static void draw_memory(uint64_t x, uint64_t y, uint64_t width,
                        uint64_t height, int memory_available,
                        int memory_limited, uint64_t free_pages,
                        uint64_t total_pages, const struct app_theme *theme) {
    app_text("MEMORIA FÍSICA GESTIONADA", x, y, 1, theme->accent);
    if (!memory_available || total_pages == 0) {
        app_text("GESTOR NO DISPONIBLE", x, y + 25, 1, theme->pending);
        app_text("DATOS LIMITADOS AL MAPA DEL ARRANQUE", x, y + 45,
                           1, theme->muted);
        return;
    }

    app_text("TOTAL:", x, y + 25, 1, theme->text);
    draw_number(total_pages / 256, x + 42 * app_text_scale, y + 25,
                theme->text);
    app_text("MIB", x + 78 * app_text_scale, y + 25, 1, theme->text);
    app_text("LIBRE:", x, y + 45, 1, theme->text);
    draw_number(free_pages / 256, x + 42 * app_text_scale, y + 45,
                theme->text);
    app_text("MIB", x + 78 * app_text_scale, y + 45, 1, theme->text);
    if (height >= 120) {
        app_text("PÁGINAS LIBRES:", x, y + 65, 1, theme->text);
        draw_number(free_pages, x + 92 * app_text_scale, y + 65, theme->text);
    }
    if (height >= 145 && width >= 120) {
        uint64_t bar_width = width - 4;
        uint64_t shown_free = free_pages > total_pages ? total_pages : free_pages;
        uint64_t free_width = shown_free * (bar_width / total_pages) +
            (shown_free * (bar_width % total_pages)) / total_pages;
        graphics_fill_rect(x, y + 90, bar_width, 10, theme->card);
        if (free_width != 0) {
            graphics_fill_rect(x, y + 90, free_width, 10, theme->success);
        }
        app_text("BARRA: PÁGINAS LIBRES", x, y + 108, 1, theme->muted);
    }
    if (height >= 170) {
        app_text(memory_limited ? "GESTIÓN PARCIAL (LÍMITE INTERNO)"
                                         : "GESTIÓN INICIAL; NO ES TODA LA RAM",
                           x, y + 128, 1, memory_limited ? theme->pending
                                                       : theme->muted);
    }
}

static void draw_services(uint64_t x, uint64_t y, uint64_t height,
                          int ps2_available, int mouse_available,
                          int memory_available, int memory_limited,
                          const struct app_theme *theme) {
    app_text("ESTADO DE LOS COMPONENTES VOS", x, y, 1, theme->accent);
    app_text("INTERFAZ GRÁFICA     ACTIVA", x, y + 24, 1, theme->success);
    app_text(ps2_available ? "ENTRADA PS/2        ACTIVA"
                                     : "ENTRADA PS/2    NO DISPONIBLE",
                       x, y + 44, 1, ps2_available ? theme->success
                                                   : theme->pending);
    if (height >= 105) {
        app_text(mouse_available ? "RATÓN PS/2         ACTIVO"
                                           : "RATÓN PS/2        OPCIONAL",
                           x, y + 64, 1, mouse_available ? theme->success
                                                       : theme->muted);
    }
    if (height >= 125) {
        app_text(memory_available ? "PÁGINAS FÍSICAS    ACTIVAS"
                                            : "PÁGINAS FÍSICAS NO DISPONIBLES",
                           x, y + 84, 1, memory_available ? theme->success
                                                        : theme->pending);
    }
    if (height >= 150 && memory_limited) {
        app_text("GESTOR DE MEMORIA CON LÍMITES INTERNOS", x, y + 104,
                           1, theme->pending);
    }
    if (height >= 175) {
        app_text("ARCHIVOS, RED, AUDIO Y USB: EN DESARROLLO",
                           x, y + 132, 1, theme->muted);
        app_text("NO HAY SERVICIOS DE FONDO NI AISLAMIENTO AÚN",
                           x, y + 152, 1, theme->muted);
    }
}

static void draw_settings(uint64_t x, uint64_t y, uint64_t width,
                          uint64_t height, int dark_mode, int accent_mode,
                          const struct app_theme *theme) {
    app_text("APARIENCIA", x, y, 1, theme->accent);
    app_text(dark_mode ? "TEMA ACTUAL: OSCURO" : "TEMA ACTUAL: CLARO",
                       x, y + 25, 1, theme->text);
    if (height >= 105) {
        app_text("CAMBIAR ENTRE TEMA CLARO Y OSCURO", x, y + 47,
                           1, theme->muted);
        uint64_t button_width = width < 190 ? width : 190;
        graphics_fill_round_rect(x, y + 70, button_width, 32, 9, theme->accent);
        struct graphics_color button_text = dark_mode
            ? theme->text : (struct graphics_color){255, 255, 255};
        app_text("CAMBIAR TEMA", x + 12, y + 82, 1, button_text);
    }
    if (height >= 175) {
        app_text(accent_mode ? "COLOR DE ACENTO: VIOLETA"
                             : "COLOR DE ACENTO: AZUL",
                 x, y + 117, 1, theme->muted);
        uint64_t button_width = width < 190 ? width : 190;
        graphics_fill_round_rect(x, y + 140, button_width, 32, 9, theme->accent);
        struct graphics_color button_text = dark_mode
            ? theme->text : (struct graphics_color){255, 255, 255};
        app_text("CAMBIAR ACENTO", x + 12, y + 151, 1, button_text);
    }
    if (height >= 205) {
        app_text("LOS CAMBIOS SOLO DURAN ESTA SESIÓN.", x,
                 y + 182, 1, theme->muted);
    }
    if (height >= 270) {
        app_text("IDIOMA ACTUAL:", x, y + 207, 1, theme->text);
        app_text(locale_name(), x + 100 * app_text_scale, y + 207,
                 1, theme->accent);
        uint64_t button_width = width < 190 ? width : 190;
        graphics_fill_round_rect(x, y + 230, button_width, 32, 9, theme->accent);
        struct graphics_color button_text = dark_mode
            ? theme->text : (struct graphics_color){255, 255, 255};
        app_text("CAMBIAR IDIOMA", x + 12, y + 241, 1, button_text);
    }
    if (height >= 335) {
        app_text("TECLADO:", x, y + 270, 1, theme->text);
        app_text(keyboard_layout_name(), x + 64 * app_text_scale,
                 y + 270, 1, theme->accent);
        uint64_t button_width = width < 190 ? width : 190;
        graphics_fill_round_rect(x, y + 293, button_width, 32, 9, theme->accent);
        struct graphics_color button_text = dark_mode
            ? theme->text : (struct graphics_color){255, 255, 255};
        app_text("CAMBIAR TECLADO", x + 12, y + 304, 1, button_text);
    }
}

static void draw_help(uint64_t x, uint64_t y, uint64_t height,
                      const struct app_theme *theme) {
    app_text("USAR VOS ES SENCILLO", x, y, 1, theme->accent);
    app_text("HAZ CLIC EN LOS ICONOS DEL DOCK.", x, y + 24, 1,
                       theme->text);
    if (height >= 105) {
        app_text("O PULSA 1-9 PARA ABRIR UNA APLICACIÓN.", x, y + 44,
                           1, theme->text);
        app_text("ESC VUELVE A INICIO; TAB CAMBIA DE APP.", x,
                           y + 64, 1, theme->text);
    }
    if (height >= 145) {
        app_text("ARRASTRA LA BARRA PARA MOVER LA VENTANA.", x,
                           y + 84, 1, theme->muted);
        app_text("EL BOTÓN VERDE MAXIMIZA O RESTAURA.", x,
                           y + 104, 1, theme->muted);
    }
    if (height >= 180) {
        app_text("VOS SIGUE EN DESARROLLO.", x, y + 130,
                           1, theme->muted);
        app_text("ALGUNAS FUNCIONES AÚN NO EXISTEN.",
                           x, y + 150, 1, theme->muted);
    }
    if (height >= 205) {
        app_text("AJUSTES: ENTER TEMA; C ACENTO; L IDIOMA; K TECLADO.",
                 x, y + 170, 1, theme->muted);
    }
    if (height >= 225) {
        app_text("ARCHIVOS RAM: N CREA; ENTER EDITA.", x, y + 190,
                 1, theme->muted);
        app_text("ESC VUELVE; SE BORRA AL APAGAR VOS.", x, y + 210,
                 1, theme->muted);
    }
    if (height >= 250) {
        app_text("APLICACIONES: PULSA T; 8 TAMBIÉN LA ABRE.", x, y + 230,
                 1, theme->muted);
    }
}

static void draw_file_content(const struct ramfs_file *file,
                              uint64_t x, uint64_t y, uint64_t width,
                              uint64_t height,
                              const struct app_theme *theme) {
    if (file == 0) return;

    graphics_draw_text(file->name, x, y, app_text_scale, theme->accent);
    app_text("ESCRIBE; ESC GUARDA LA NOTA EN RAM", x, y + 18 * app_text_scale,
             1, theme->muted);
    uint64_t max_columns = width / (6 * app_text_scale);
    if (max_columns > 120) max_columns = 120;
    if (max_columns == 0) return;

    char line[121];
    uint64_t position = 0;
    uint64_t line_y = y + 42 * app_text_scale;
    uint64_t line_height = 10 * app_text_scale;
    while (position < file->length &&
           line_y + 7 * app_text_scale <= y + height) {
        uint64_t length = 0;
        uint64_t columns = 0;
        while (position < file->length && columns < max_columns) {
            uint8_t first = (uint8_t)file->content[position];
            if (first == '\n') {
                position++;
                break;
            }
            uint32_t sequence = first < 0x80 ? 1u
                : ((first & 0xe0u) == 0xc0u ? 2u
                : ((first & 0xf0u) == 0xe0u ? 3u
                : ((first & 0xf8u) == 0xf0u ? 4u : 1u)));
            if (position + sequence > file->length) sequence = 1;
            if (length + sequence >= sizeof(line)) break;
            for (uint32_t i = 0; i < sequence; i++) {
                line[length++] = file->content[position++];
            }
            columns++;
        }
        line[length] = '\0';
        graphics_draw_text(line, x, line_y, app_text_scale, theme->text);
        line_y += line_height;
    }
}

static void draw_files(uint64_t x, uint64_t y, uint64_t width,
                       uint64_t height, const struct app_theme *theme) {
    if (files_editing) {
        draw_file_content(ramfs_get(editing_file), x, y, width, height, theme);
        return;
    }

    app_text("ARCHIVOS DE VOS EN RAM", x, y, 1, theme->accent);
    app_text("TEMPORALES: SE BORRAN AL APAGAR VOS", x,
             y + 20 * app_text_scale, 1, theme->muted);
    uint64_t row_height = 22 * app_text_scale;
    uint64_t row_y = y + 42 * app_text_scale;
    visible_file_rows = height > 72 * app_text_scale
        ? (height - 72 * app_text_scale) / row_height : 0;
    uint32_t count = ramfs_count();
    if (file_scroll > count) file_scroll = count;
    if (selected_file >= count && count != 0) select_file(count - 1);
    for (uint32_t row = 0; row < visible_file_rows &&
         file_scroll + row < count; row++) {
        uint32_t i = file_scroll + row;
        const struct ramfs_file *file = ramfs_get(i);
        if (file == 0) continue;
        if (i == selected_file) {
            graphics_fill_rect(x, row_y, width, 20 * app_text_scale,
                               theme->card);
            graphics_fill_rect(x, row_y, 3 * app_text_scale,
                               20 * app_text_scale, theme->accent);
        }
        graphics_draw_text(file->name, x + 10 * app_text_scale,
                           row_y + 5 * app_text_scale, app_text_scale,
                           theme->text);
        row_y += row_height;
    }

    uint64_t instruction_y = y + height > 14 * app_text_scale
        ? y + height - 12 * app_text_scale : y;
    app_text("N: NUEVA  FLECHAS: ELEGIR  ENTER: EDITAR",
             x, instruction_y, 1, theme->muted);
    if (instruction_y >= 12 * app_text_scale) {
        app_text("ESC: VOLVER", x, instruction_y - 12 * app_text_scale,
                 1, theme->muted);
    }
    if (count >= ramfs_capacity()) {
        app_text("LIMITE: 8 ARCHIVOS PEQUENOS EN RAM", x,
                 instruction_y >= 12 * app_text_scale
                    ? instruction_y - 12 * app_text_scale : y,
                 1, theme->pending);
    }
}

void apps_draw_content(enum vos_app_id app, uint64_t x, uint64_t y,
                       uint64_t width, uint64_t height,
                       const struct limine_framebuffer *framebuffer,
                       uint32_t text_scale,
                       int ps2_available, int mouse_available,
                       int memory_available, int memory_limited,
                       uint64_t free_pages, uint64_t total_pages,
                       int dark_mode, int accent_mode,
                       const struct app_theme *theme) {
    if (theme == 0 || width == 0 || height == 0) {
        return;
    }
    app_text_scale = text_scale < 1 ? 1 : (text_scale > 2 ? 2 : text_scale);

    switch (app) {
        case VOS_APP_HOME:
            draw_home(x, y, width, height, theme);
            break;
        case VOS_APP_SYSTEM:
            draw_system(x, y, width, height, framebuffer, ps2_available,
                        mouse_available, theme);
            break;
        case VOS_APP_MEMORY:
            draw_memory(x, y, width, height, memory_available, memory_limited,
                        free_pages, total_pages, theme);
            break;
        case VOS_APP_SERVICES:
            draw_services(x, y, height, ps2_available, mouse_available,
                          memory_available, memory_limited, theme);
            break;
        case VOS_APP_SETTINGS:
            draw_settings(x, y, width, height, dark_mode, accent_mode, theme);
            break;
        case VOS_APP_HELP:
            draw_help(x, y, height, theme);
            break;
        case VOS_APP_FILES:
            draw_files(x, y, width, height, theme);
            break;
        case VOS_APP_STORE:
            draw_store(x, y, width, height, theme);
            break;
        case VOS_APP_CALCULATOR:
            draw_calculator(x, y, width, height, theme);
            break;
        default:
            draw_home(x, y, width, height, theme);
            break;
    }
}

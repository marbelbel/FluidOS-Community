#include "keyboard.h"
#include <stdint.h>

static const char unshifted_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0a] = '9', [0x0b] = '0', [0x0c] = '-', [0x0d] = '=',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1a] = '[', [0x1b] = ']',
    [0x1e] = 'a', [0x1f] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2b] = '\\', [0x2c] = 'z', [0x2d] = 'x', [0x2e] = 'c',
    [0x2f] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/', [0x39] = ' '
};

static const char shifted_map[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0a] = '(', [0x0b] = ')', [0x0c] = '_', [0x0d] = '+',
    [0x1a] = '{', [0x1b] = '}', [0x27] = ':', [0x28] = '"',
    [0x29] = '~', [0x2b] = '|', [0x33] = '<', [0x34] = '>',
    [0x35] = '?'
};

static uint8_t left_shift;
static uint8_t right_shift;
static uint8_t left_control;
static uint8_t right_control;
static uint8_t left_alt;
static uint8_t right_alt;
static uint8_t caps_lock;
static uint8_t extended_prefix;
static uint8_t pause_bytes_remaining;
static uint8_t pending_accent;
static enum keyboard_layout active_layout = KEYBOARD_LAYOUT_SPANISH;

static const uint32_t spanish_unshifted_map[128] = {
    [0x0c] = '\'', [0x0d] = 0x00a1, [0x1a] = '`', [0x1b] = '+',
    [0x27] = 0x00f1, [0x28] = 0x00b4, [0x29] = 0x00ba,
    [0x2b] = 0x00e7, [0x33] = ',', [0x34] = '.', [0x35] = '-'
};

static const uint32_t spanish_shifted_map[128] = {
    [0x03] = '"', [0x04] = 0x00b7, [0x07] = '&', [0x08] = '/',
    [0x09] = '(', [0x0a] = ')', [0x0b] = '=', [0x0c] = '?',
    [0x0d] = 0x00bf, [0x1a] = '^', [0x1b] = '*',
    [0x27] = 0x00d1, [0x28] = 0x00a8, [0x29] = 0x00aa,
    [0x2b] = 0x00c7,
    [0x33] = ';', [0x34] = ':', [0x35] = '_'
};

static const uint32_t uk_shifted_map[128] = {
    [0x03] = '"', [0x04] = 0x00a3, [0x08] = '&', [0x09] = '*',
    [0x0a] = '(', [0x0b] = ')', [0x28] = '@', [0x29] = 0x00ac,
    [0x2b] = '~'
};

static const uint32_t uk_unshifted_map[128] = {
    [0x2b] = '#'
};

static const uint32_t french_unshifted_map[128] = {
    [0x02] = '&', [0x03] = 0x00e9, [0x04] = '"', [0x05] = '\'',
    [0x06] = '(', [0x07] = '-', [0x08] = 0x00e8, [0x09] = '_',
    [0x0a] = 0x00e7, [0x0b] = 0x00e0, [0x0c] = ')', [0x0d] = '=',
    [0x10] = 'a', [0x11] = 'z', [0x1a] = '^', [0x1b] = '$',
    [0x1e] = 'q', [0x27] = 'm', [0x28] = 0x00f9, [0x29] = 0x00b2,
    [0x2b] = '*', [0x2c] = 'w', [0x32] = ',', [0x33] = ';',
    [0x34] = ':', [0x35] = '!'
};

static const uint32_t french_shifted_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0a] = '9', [0x0b] = '0', [0x0c] = 0x00b0, [0x0d] = '+',
    [0x1a] = 0x00a8, [0x1b] = 0x00a3, [0x28] = '%', [0x29] = 0x00b3,
    [0x2b] = 0x00b5, [0x32] = '?', [0x33] = '.', [0x34] = '/',
    [0x35] = 0x00a7
};

static const uint32_t german_unshifted_map[128] = {
    [0x0c] = 0x00df, [0x0d] = 0x00b4, [0x15] = 'z', [0x1a] = 0x00fc,
    [0x1b] = '+', [0x27] = 0x00f6, [0x28] = 0x00e4, [0x29] = '^',
    [0x2b] = '#', [0x2c] = 'y', [0x56] = '<'
};

static const uint32_t german_shifted_map[128] = {
    [0x0c] = '?', [0x0d] = '`', [0x1a] = 0x00dc, [0x1b] = '*',
    [0x27] = 0x00d6, [0x28] = 0x00c4, [0x29] = 0x00b0,
    [0x2b] = '\'', [0x56] = '>'
};

static uint32_t layout_character(uint8_t scan_code, uint8_t shifted) {
    const uint32_t *layout_map = 0;
    uint32_t fallback = shifted ? (uint8_t)shifted_map[scan_code]
                                : (uint8_t)unshifted_map[scan_code];

    switch (active_layout) {
        case KEYBOARD_LAYOUT_SPANISH:
            layout_map = shifted ? spanish_shifted_map : spanish_unshifted_map;
            break;
        case KEYBOARD_LAYOUT_UK:
            layout_map = shifted ? uk_shifted_map : uk_unshifted_map;
            break;
        case KEYBOARD_LAYOUT_FRENCH:
            layout_map = shifted ? french_shifted_map : french_unshifted_map;
            break;
        case KEYBOARD_LAYOUT_GERMAN:
            layout_map = shifted ? german_shifted_map : german_unshifted_map;
            break;
        case KEYBOARD_LAYOUT_US:
        default:
            break;
    }

    if (layout_map != 0 && layout_map[scan_code] != 0) {
        return layout_map[scan_code];
    }
    return fallback;
}

static uint8_t current_modifiers(void) {
    uint8_t modifiers = 0;
    if (left_shift || right_shift) modifiers |= INPUT_MOD_SHIFT;
    if (left_control || right_control) modifiers |= INPUT_MOD_CONTROL;
    if (left_alt || right_alt) modifiers |= INPUT_MOD_ALT;
    if (caps_lock) modifiers |= INPUT_MOD_CAPS_LOCK;
    return modifiers;
}

static uint32_t compose_accent(uint32_t character, uint8_t accent) {
    if (accent == 1) {
        switch (character) {
            case 'a': return 0x00e1; case 'e': return 0x00e9;
            case 'i': return 0x00ed; case 'o': return 0x00f3;
            case 'u': return 0x00fa; case 'A': return 0x00c1;
            case 'E': return 0x00c9; case 'I': return 0x00cd;
            case 'O': return 0x00d3; case 'U': return 0x00da;
            default: return character;
        }
    }
    if (accent == 2) {
        if (character == 'u') return 0x00fc;
        if (character == 'U') return 0x00dc;
    }
    return character;
}

static uint32_t translate_character(uint8_t scan_code, uint8_t extended) {
    if (extended) {
        if (scan_code == 0x1c) return '\n';
        if (scan_code == 0x35) return '/';
        return 0;
    }

    if (scan_code == 0x1c) return '\n';
    if (scan_code == 0x0e) return '\b';

    if (active_layout == KEYBOARD_LAYOUT_SPANISH && scan_code == 0x28) {
        pending_accent = (left_shift || right_shift) ? 2 : 1;
        return 0;
    }

    uint32_t plain = layout_character(scan_code, 0);
    uint8_t uppercase = ((left_shift || right_shift) != 0) ^ (caps_lock != 0);
    if (plain >= 'a' && plain <= 'z') {
        if (uppercase) {
            plain = plain - 'a' + 'A';
        }
    } else if (((plain >= 0x00e0 && plain <= 0x00f6) ||
                (plain >= 0x00f8 && plain <= 0x00fe)) && uppercase) {
        plain -= 0x20;
    }

    if (left_shift || right_shift) {
        uint32_t shifted = layout_character(scan_code, 1);
        if (shifted != 0) plain = shifted;
    }
    if (pending_accent != 0) {
        plain = compose_accent(plain, pending_accent);
        pending_accent = 0;
    }
    return plain;
}

void keyboard_set_layout(enum keyboard_layout layout) {
    if ((uint32_t)layout < (uint32_t)KEYBOARD_LAYOUT_COUNT) {
        active_layout = layout;
        pending_accent = 0;
    }
}

void keyboard_toggle_layout(void) {
    uint32_t next_layout = ((uint32_t)active_layout + 1u) %
                           (uint32_t)KEYBOARD_LAYOUT_COUNT;
    keyboard_set_layout((enum keyboard_layout)next_layout);
}

enum keyboard_layout keyboard_current_layout(void) {
    return active_layout;
}

const char *keyboard_layout_name(void) {
    switch (active_layout) {
        case KEYBOARD_LAYOUT_SPANISH: return "ES QWERTY";
        case KEYBOARD_LAYOUT_UK: return "UK QWERTY";
        case KEYBOARD_LAYOUT_FRENCH: return "FR AZERTY";
        case KEYBOARD_LAYOUT_GERMAN: return "DE QWERTZ";
        case KEYBOARD_LAYOUT_US:
        default: return "US QWERTY";
    }
}

static void update_modifiers(uint8_t scan_code, uint8_t extended,
                             uint8_t pressed) {
    switch (scan_code) {
        case 0x2a: if (!extended) left_shift = pressed; break;
        case 0x36: if (!extended) right_shift = pressed; break;
        case 0x1d:
            if (extended) right_control = pressed;
            else left_control = pressed;
            break;
        case 0x38:
            if (extended) right_alt = pressed;
            else left_alt = pressed;
            break;
        case 0x3a:
            if (pressed && !extended) caps_lock ^= 1;
            break;
    }
}

static uint16_t key_code(uint8_t scan_code, uint8_t extended) {
    uint16_t code = extended ? (uint16_t)(0x100 | scan_code) : scan_code;
    if (!extended) return code;

    switch (scan_code) {
        case 0x48: return INPUT_KEY_UP;
        case 0x50: return INPUT_KEY_DOWN;
        case 0x4b: return INPUT_KEY_LEFT;
        case 0x4d: return INPUT_KEY_RIGHT;
        case 0x47: return INPUT_KEY_HOME;
        case 0x4f: return INPUT_KEY_END;
        case 0x49: return INPUT_KEY_PAGE_UP;
        case 0x51: return INPUT_KEY_PAGE_DOWN;
        case 0x53: return INPUT_KEY_DELETE;
        case 0x1c: return INPUT_KEY_ENTER;
        case 0x1d: return INPUT_KEY_CONTROL;
        case 0x38: return INPUT_KEY_ALT;
        default: return code;
    }
}

int keyboard_process_scancode(uint8_t scan, struct input_event *event) {
    if (event == 0) {
        return 0;
    }
    if (pause_bytes_remaining != 0) {
        pause_bytes_remaining--;
        return 0;
    }
    if (scan == 0xe1) {
        extended_prefix = 0;
        pause_bytes_remaining = 5;
        return 0;
    }
    if (scan == 0xe0) {
        extended_prefix = 1;
        return 0;
    }

    uint8_t extended = extended_prefix;
    extended_prefix = 0;
    uint8_t pressed = (scan & 0x80) == 0;
    uint8_t scan_code = scan & 0x7f;
    update_modifiers(scan_code, extended, pressed);

    event->type = INPUT_EVENT_KEY;
    event->key = key_code(scan_code, extended);
    event->pressed = pressed;
    event->modifiers = current_modifiers();
    event->character = pressed ? translate_character(scan_code, extended) : 0;
    event->mouse_x = 0;
    event->mouse_y = 0;
    event->mouse_delta_x = 0;
    event->mouse_delta_y = 0;
    event->mouse_buttons = 0;
    event->mouse_buttons_changed = 0;
    return 1;
}


#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "keyboard.h"

static uint32_t failures;

static void expect_character(const char *name, uint32_t expected,
                             uint32_t actual) {
    if (expected == actual) return;
    printf("FAIL: %s (esperado U+%04X, recibido U+%04X)\n",
           name, (unsigned)expected, (unsigned)actual);
    failures++;
}

static uint32_t press(uint8_t scan_code) {
    struct input_event event = {0};
    if (!keyboard_process_scancode(scan_code, &event)) return UINT32_MAX;
    return event.character;
}

int main(void) {
    keyboard_set_layout(KEYBOARD_LAYOUT_SPANISH);
    expect_character("ES: tecla ñ", 0x00f1, press(0x27));
    press(0x2a);
    expect_character("ES: Shift+ñ", 0x00d1, press(0x27));
    expect_character("ES: Shift+ç produce Ç", 0x00c7, press(0x2b));
    press(0xaa);
    expect_character("ES: tecla ç", 0x00e7, press(0x2b));

    keyboard_set_layout(KEYBOARD_LAYOUT_US);
    expect_character("US: QWERTY", 'q', press(0x10));

    keyboard_set_layout(KEYBOARD_LAYOUT_UK);
    press(0x2a);
    expect_character("UK: Shift+2", '"', press(0x03));
    press(0xaa);

    keyboard_set_layout(KEYBOARD_LAYOUT_FRENCH);
    expect_character("FR: AZERTY a", 'a', press(0x10));
    expect_character("FR: AZERTY q", 'q', press(0x1e));
    expect_character("FR: e-acute", 0x00e9, press(0x03));
    expect_character("FR: c-cedilla", 0x00e7, press(0x0a));
    press(0x3a);
    expect_character("FR: Bloq Mayús + ç produce Ç", 0x00c7, press(0x0a));
    press(0x3a);

    keyboard_set_layout(KEYBOARD_LAYOUT_GERMAN);
    expect_character("DE: QWERTZ y", 'y', press(0x2c));
    expect_character("DE: u-umlaut", 0x00fc, press(0x1a));
    press(0x2a);
    expect_character("DE: Shift+u-umlaut", 0x00dc, press(0x1a));
    press(0xaa);

    keyboard_set_layout(KEYBOARD_LAYOUT_SPANISH);
    for (uint32_t i = 0; i < (uint32_t)KEYBOARD_LAYOUT_COUNT; i++) {
        keyboard_toggle_layout();
    }
    if (keyboard_current_layout() != KEYBOARD_LAYOUT_SPANISH) {
        printf("FAIL: el ciclo de distribuciones no vuelve al inicio\n");
        failures++;
    }

    if (!cpu_vendor_id_is_intel("GenuineIntel")) {
        printf("FAIL: identifica GenuineIntel\n");
        failures++;
    }
    if (cpu_vendor_id_is_intel("AuthenticAMD") ||
        cpu_vendor_id_is_intel("GenuineIbm")) {
        printf("FAIL: no rechaza otro fabricante de CPU\n");
        failures++;
    }

    if (failures != 0) return 1;
    puts("OK: CPU Intel y 5 mapas PS/2 (ES, US, UK, FR, DE)");
    return 0;
}

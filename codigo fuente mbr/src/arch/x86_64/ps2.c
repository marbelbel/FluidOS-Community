#include "ps2.h"

#define PS2_STATUS_PORT 0x64
#define PS2_DATA_PORT 0x60
#define PS2_WAIT_LIMIT 100000

static uint8_t port_in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void port_out8(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static int wait_input_empty(void) {
    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; i++) {
        if ((port_in8(PS2_STATUS_PORT) & 0x02) == 0) {
            return 1;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

static int controller_command(uint8_t command) {
    if (!wait_input_empty()) {
        return 0;
    }
    port_out8(PS2_STATUS_PORT, command);
    return 1;
}

static int controller_write_data(uint8_t value) {
    if (!wait_input_empty()) {
        return 0;
    }
    port_out8(PS2_DATA_PORT, value);
    return 1;
}

static int controller_read_data(uint8_t *value) {
    if (value == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; i++) {
        uint8_t status = port_in8(PS2_STATUS_PORT);
        if ((status & 0x01) == 0) {
            __asm__ volatile ("pause");
            continue;
        }
        uint8_t response = port_in8(PS2_DATA_PORT);
        if ((status & 0x20) == 0) {
            *value = response;
            return 1;
        }
    }
    return 0;
}

static void discard_pending_output(void) {
    for (uint32_t i = 0; i < 256; i++) {
        if ((port_in8(PS2_STATUS_PORT) & 0x01) == 0) {
            break;
        }
        (void)port_in8(PS2_DATA_PORT);
    }
}

int ps2_init(void) {
    uint8_t configuration;

    /* Disable both channels before changing the controller configuration. */
    if (!controller_command(0xad)) {
        return 0;
    }
    if (!controller_command(0xa7)) {
        (void)controller_command(0xae);
        return 0;
    }
    discard_pending_output();
    if (!controller_command(0x20) || !controller_read_data(&configuration)) {
        (void)controller_command(0xae);
        return 0;
    }

    /* Polling is used for now, so keep controller IRQs off and clocks on. */
    configuration &= (uint8_t)~0x33;
    if (!controller_command(0x60) || !controller_write_data(configuration) ||
        !controller_command(0xae)) {
        (void)controller_command(0xae);
        return 0;
    }
    /* A missing second channel only disables mouse support, not the keyboard. */
    (void)controller_command(0xa8);
    return 1;
}

int ps2_read_byte(uint8_t *value, enum ps2_device *device) {
    if (value == 0 || device == 0) {
        return 0;
    }

    uint8_t status = port_in8(PS2_STATUS_PORT);
    if ((status & 0x01) == 0) {
        return 0;
    }

    *value = port_in8(PS2_DATA_PORT);
    *device = (status & 0x20) != 0 ? PS2_DEVICE_MOUSE : PS2_DEVICE_KEYBOARD;
    if ((status & 0xc0) != 0) {
        return 0;
    }
    return 1;
}

int ps2_mouse_command(uint8_t command) {
    for (uint32_t retry = 0; retry < 3; retry++) {
        if (!controller_command(0xd4) || !controller_write_data(command)) {
            return 0;
        }

        int resend = 0;
        for (uint32_t attempt = 0; attempt < PS2_WAIT_LIMIT; attempt++) {
            uint8_t status = port_in8(PS2_STATUS_PORT);
            if ((status & 0x01) == 0) {
                __asm__ volatile ("pause");
                continue;
            }

            uint8_t response = port_in8(PS2_DATA_PORT);
            if ((status & 0x20) != 0) {
                if (response == 0xfa) {
                    return 1;
                }
                if (response == 0xfe) {
                    resend = 1;
                    break;
                }
                return 0;
            }
            /* Discard unrelated keyboard input during this short startup exchange. */
        }
        if (!resend) {
            return 0;
        }
    }
    return 0;
}

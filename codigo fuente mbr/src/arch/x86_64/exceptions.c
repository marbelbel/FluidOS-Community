#include "exceptions.h"
#include "graphics.h"
#include <stdint.h>

struct exception_frame {
    uint64_t instruction_pointer;
    uint64_t code_segment;
    uint64_t flags;
    uint64_t stack_pointer;
    uint64_t stack_segment;
};

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed));

_Static_assert(sizeof(struct idt_entry) == 16, "IDT gate must be 16 bytes");
_Static_assert(sizeof(struct idtr) == 10, "IDTR must be 10 bytes in long mode");

static struct idt_entry idt[256] __attribute__((aligned(16)));
static volatile uint8_t panic_in_progress;

static void draw_hex64(uint64_t value, uint64_t x, uint64_t y,
                       struct graphics_color color) {
    static const char hex_digits[] = "0123456789ABCDEF";
    for (uint32_t digit = 0; digit < 16; digit++) {
        uint32_t shift = (15 - digit) * 4;
        graphics_draw_char(hex_digits[(value >> shift) & 0x0f],
                           x + digit * 6, y, 1, color);
    }
}

__attribute__((noreturn))
static void exception_stop(uint16_t vector, uint64_t error_code,
                           const struct exception_frame *frame) {
    if (panic_in_progress) {
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    panic_in_progress = 1;

    uint64_t fault_address = 0;
    if (vector == 14) {
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
    }

    const struct graphics_color background = {18, 29, 48};
    const struct graphics_color foreground = {245, 248, 255};
    const struct graphics_color accent = {120, 177, 255};
    graphics_clear(background);
    uint64_t numbers[3] = {vector, error_code, frame->instruction_pointer};
    const char *labels[3] = {"VECTOR:", "ERROR:", "RIP:"};
    graphics_draw_text("VOS SE HA DETENIDO", 24, 18, 2, foreground);
    graphics_draw_text("FALLO INTERNO DEL PROCESADOR", 24, 45, 1, accent);
    for (uint32_t item = 0; item < 3; item++) {
        uint64_t y = 68 + item * 20;
        graphics_draw_text(labels[item], 24, y, 1, foreground);
        draw_hex64(numbers[item], 72, y, foreground);
    }
    if (vector == 14) {
        graphics_draw_text("CR2:", 24, 130, 1, foreground);
        draw_hex64(fault_address, 72, 130, foreground);
    }
    graphics_draw_text("EL KERNEL SE DETUVO PARA EVITAR ERRORES.",
                       24, 158, 1, foreground);
    graphics_draw_text("ANOTA ESTOS DATOS PARA REVISAR EL FALLO.",
                       24, 176, 1, foreground);
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

#define DEFINE_NO_ERROR_HANDLER(number) \
    static void exception_##number(struct exception_frame *frame) \
        __attribute__((interrupt)); \
    static void exception_##number(struct exception_frame *frame) { \
        exception_stop(number, 0, frame); \
    }

#define DEFINE_ERROR_HANDLER(number) \
    static void exception_##number(struct exception_frame *frame, \
                                   unsigned long long error_code) \
        __attribute__((interrupt)); \
    static void exception_##number(struct exception_frame *frame, \
                                   unsigned long long error_code) { \
        exception_stop(number, error_code, frame); \
    }

/* Error-code handlers follow the x86 exception frame rules. */
DEFINE_NO_ERROR_HANDLER(0)
DEFINE_NO_ERROR_HANDLER(1)
DEFINE_NO_ERROR_HANDLER(2)
DEFINE_NO_ERROR_HANDLER(3)
DEFINE_NO_ERROR_HANDLER(4)
DEFINE_NO_ERROR_HANDLER(5)
DEFINE_NO_ERROR_HANDLER(6)
DEFINE_NO_ERROR_HANDLER(7)
DEFINE_ERROR_HANDLER(8)
DEFINE_NO_ERROR_HANDLER(9)
DEFINE_ERROR_HANDLER(10)
DEFINE_ERROR_HANDLER(11)
DEFINE_ERROR_HANDLER(12)
DEFINE_ERROR_HANDLER(13)
DEFINE_ERROR_HANDLER(14)
DEFINE_NO_ERROR_HANDLER(15)
DEFINE_NO_ERROR_HANDLER(16)
DEFINE_ERROR_HANDLER(17)
DEFINE_NO_ERROR_HANDLER(18)
DEFINE_NO_ERROR_HANDLER(19)
DEFINE_NO_ERROR_HANDLER(20)
DEFINE_ERROR_HANDLER(21)
DEFINE_NO_ERROR_HANDLER(22)
DEFINE_NO_ERROR_HANDLER(23)
DEFINE_NO_ERROR_HANDLER(24)
DEFINE_NO_ERROR_HANDLER(25)
DEFINE_NO_ERROR_HANDLER(26)
DEFINE_NO_ERROR_HANDLER(27)
DEFINE_NO_ERROR_HANDLER(28)
DEFINE_ERROR_HANDLER(29)
DEFINE_ERROR_HANDLER(30)
DEFINE_NO_ERROR_HANDLER(31)

DEFINE_NO_ERROR_HANDLER(255)

static void set_gate(uint16_t vector, uintptr_t handler, uint16_t selector) {
    idt[vector].offset_low = (uint16_t)(handler & 0xffff);
    idt[vector].selector = selector;
    idt[vector].ist = 0;
    idt[vector].attributes = 0x8e;
    idt[vector].offset_middle = (uint16_t)((handler >> 16) & 0xffff);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xffffffff);
    idt[vector].reserved = 0;
}

int exceptions_install(void) {
    uint16_t code_segment;
    __asm__ volatile ("mov %%cs, %0" : "=r"(code_segment));

    for (uint16_t vector = 0; vector < 256; vector++) {
        set_gate(vector, (uintptr_t)exception_255, code_segment);
    }

#define INSTALL_HANDLER(number) \
    set_gate(number, (uintptr_t)exception_##number, code_segment)

    INSTALL_HANDLER(0);
    INSTALL_HANDLER(1);
    INSTALL_HANDLER(2);
    INSTALL_HANDLER(3);
    INSTALL_HANDLER(4);
    INSTALL_HANDLER(5);
    INSTALL_HANDLER(6);
    INSTALL_HANDLER(7);
    INSTALL_HANDLER(8);
    INSTALL_HANDLER(9);
    INSTALL_HANDLER(10);
    INSTALL_HANDLER(11);
    INSTALL_HANDLER(12);
    INSTALL_HANDLER(13);
    INSTALL_HANDLER(14);
    INSTALL_HANDLER(15);
    INSTALL_HANDLER(16);
    INSTALL_HANDLER(17);
    INSTALL_HANDLER(18);
    INSTALL_HANDLER(19);
    INSTALL_HANDLER(20);
    INSTALL_HANDLER(21);
    INSTALL_HANDLER(22);
    INSTALL_HANDLER(23);
    INSTALL_HANDLER(24);
    INSTALL_HANDLER(25);
    INSTALL_HANDLER(26);
    INSTALL_HANDLER(27);
    INSTALL_HANDLER(28);
    INSTALL_HANDLER(29);
    INSTALL_HANDLER(30);
    INSTALL_HANDLER(31);

#undef INSTALL_HANDLER

    struct idtr descriptor = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base = (uintptr_t)idt
    };
    __asm__ volatile ("lidt %0" : : "m"(descriptor));
    return 1;
}

#include "desktop.h"
#include "block.h"
#include "boot_splash.h"
#include "cpu.h"
#include "exceptions.h"
#include "graphics.h"
#include "input.h"
#include "kernel.h"
#include "limine.h"
#include "pmm.h"

__attribute__((used, section(".limine_requests_start"), aligned(8)))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile struct limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests_end"), aligned(8)))
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

static __attribute__((noreturn)) void stop_cpu(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static __attribute__((noreturn)) void show_boot_error(void) {
    boot_splash_error();
    stop_cpu();
}

__attribute__((noreturn)) void kernel_main(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
        stop_cpu();
    }

    struct limine_framebuffer_response *framebuffer_response =
        framebuffer_request.response;
    if (framebuffer_response == 0 || framebuffer_response->framebuffer_count == 0 ||
        framebuffer_response->framebuffers == 0) {
        stop_cpu();
    }

    struct limine_framebuffer *framebuffer =
        framebuffer_response->framebuffers[0];
    if (!graphics_init(framebuffer)) {
        stop_cpu();
    }

    boot_splash_draw(0);
    if (!exceptions_install()) {
        show_boot_error();
    }
    if (!cpu_is_intel()) {
        show_boot_error();
    }
    boot_splash_draw(1);

    block_init();
    struct limine_memmap_response *memory_map = memory_map_request.response;
    struct limine_hhdm_response *hhdm = hhdm_request.response;
    (void)pmm_init(memory_map, hhdm);
    boot_splash_draw(2);

    /* A missing PS/2 controller must not prevent the graphical desktop. */
    (void)input_init(framebuffer->width, framebuffer->height);

    boot_splash_draw(3);
    desktop_main(framebuffer);
}

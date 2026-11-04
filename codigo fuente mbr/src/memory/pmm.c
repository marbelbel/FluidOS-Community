#include "pmm.h"

#define PAGE_SIZE 4096ULL
#define PAGE_MASK (PAGE_SIZE - 1)
#define MAX_MAP_ENTRIES 256
#define MAX_USABLE_RANGES 128
#define MAX_TRACKED_PAGES (4ULL * 1024 * 1024)

struct page_range {
    uint64_t base;
    uint64_t next;
    uint64_t scan;
    uint64_t end;
    uint64_t first_slot;
};

static struct page_range ranges[MAX_USABLE_RANGES];
static uint8_t allocation_bitmap[MAX_TRACKED_PAGES / 8];
static uint64_t range_count;
static uint64_t total_pages;
static uint64_t free_pages;
static uint64_t hhdm_offset;
static int ready;
static int limited;

static int page_is_allocated(uint64_t slot) {
    return (allocation_bitmap[slot / 8] & (uint8_t)(1u << (slot % 8))) != 0;
}

static void set_page_allocated(uint64_t slot) {
    allocation_bitmap[slot / 8] |= (uint8_t)(1u << (slot % 8));
}

static void set_page_free(uint64_t slot) {
    allocation_bitmap[slot / 8] &= (uint8_t)~(1u << (slot % 8));
}

static int add_usable_range(const struct limine_memmap_entry *entry) {
    if (entry == 0 || entry->type != LIMINE_MEMMAP_USABLE ||
        entry->length == 0 || entry->base > UINT64_MAX - entry->length) {
        return 0;
    }

    uint64_t end = entry->base + entry->length;
    if (entry->base > UINT64_MAX - PAGE_MASK) {
        return 0;
    }
    uint64_t start = (entry->base + PAGE_MASK) & ~PAGE_MASK;
    end &= ~PAGE_MASK;

    /* Keep the null page reserved even if the firmware marks it usable. */
    if (start < PAGE_SIZE) {
        start = PAGE_SIZE;
    }
    if (end <= start || range_count == MAX_USABLE_RANGES) {
        return 0;
    }

    uint64_t page_count = (end - start) / PAGE_SIZE;
    if (page_count == 0 || hhdm_offset > UINTPTR_MAX - (end - 1)) {
        return 0;
    }

    for (uint64_t i = 0; i < range_count; i++) {
        if (start < ranges[i].end && ranges[i].base < end) {
            limited = 1;
            return 0;
        }
    }

    if (total_pages >= MAX_TRACKED_PAGES) {
        limited = 1;
        return 0;
    }
    if (page_count > MAX_TRACKED_PAGES - total_pages) {
        page_count = MAX_TRACKED_PAGES - total_pages;
        end = start + page_count * PAGE_SIZE;
        limited = 1;
    }

    ranges[range_count].base = start;
    ranges[range_count].next = start;
    ranges[range_count].scan = start;
    ranges[range_count].end = end;
    ranges[range_count].first_slot = total_pages;
    range_count++;
    total_pages += page_count;
    free_pages += page_count;
    return 1;
}

int pmm_init(const struct limine_memmap_response *memory_map,
             const struct limine_hhdm_response *hhdm) {
    /* Never reset the allocator after pages have been handed to the kernel. */
    if (ready) {
        return 1;
    }

    range_count = 0;
    total_pages = 0;
    free_pages = 0;
    hhdm_offset = 0;
    ready = 0;
    limited = 0;
    for (uint64_t i = 0; i < sizeof(allocation_bitmap); i++) {
        allocation_bitmap[i] = 0;
    }

    if (memory_map == 0 || memory_map->entries == 0 || hhdm == 0) {
        return 0;
    }

    hhdm_offset = hhdm->offset;
    uint64_t entries_to_read = memory_map->entry_count;
    if (entries_to_read > MAX_MAP_ENTRIES) {
        entries_to_read = MAX_MAP_ENTRIES;
        limited = 1;
    }

    for (uint64_t i = 0; i < entries_to_read; i++) {
        const struct limine_memmap_entry *entry = memory_map->entries[i];
        if (entry != 0 && entry->type == LIMINE_MEMMAP_USABLE &&
            range_count == MAX_USABLE_RANGES) {
            limited = 1;
            break;
        }
        (void)add_usable_range(entry);
    }

    ready = range_count != 0;
    return ready;
}

int pmm_alloc_page(uint64_t *physical_address, void **direct_map_address) {
    if (!ready || physical_address == 0 || direct_map_address == 0 ||
        free_pages == 0) {
        return 0;
    }

    for (uint64_t i = 0; i < range_count; i++) {
        struct page_range *range = &ranges[i];
        while (range->scan < range->next) {
            uint64_t physical = range->scan;
            range->scan += PAGE_SIZE;
            uint64_t slot = range->first_slot +
                (physical - range->base) / PAGE_SIZE;
            if (page_is_allocated(slot)) {
                continue;
            }

            set_page_allocated(slot);
            free_pages--;
            uintptr_t virtual_address = (uintptr_t)(hhdm_offset + physical);
            volatile uint8_t *page = (volatile uint8_t *)virtual_address;
            for (uint64_t byte = 0; byte < PAGE_SIZE; byte++) {
                page[byte] = 0;
            }
            *physical_address = physical;
            *direct_map_address = (void *)virtual_address;
            return 1;
        }

        if (range->next >= range->end) {
            continue;
        }

        uint64_t physical = range->next;
        range->next += PAGE_SIZE;
        range->scan = range->next;
        uint64_t slot = range->first_slot +
            (physical - range->base) / PAGE_SIZE;
        set_page_allocated(slot);
        free_pages--;

        uintptr_t virtual_address = (uintptr_t)(hhdm_offset + physical);
        volatile uint8_t *page = (volatile uint8_t *)virtual_address;
        for (uint64_t byte = 0; byte < PAGE_SIZE; byte++) {
            page[byte] = 0;
        }

        *physical_address = physical;
        *direct_map_address = (void *)virtual_address;
        return 1;
    }

    /* The counters and ranges must agree; fail closed if they do not. */
    free_pages = 0;
    return 0;
}

int pmm_free_page(uint64_t physical_address) {
    if (!ready || (physical_address & PAGE_MASK) != 0) {
        return 0;
    }

    for (uint64_t i = 0; i < range_count; i++) {
        struct page_range *range = &ranges[i];
        if (physical_address < range->base || physical_address >= range->next) {
            continue;
        }

        uint64_t slot = range->first_slot +
            (physical_address - range->base) / PAGE_SIZE;
        if (!page_is_allocated(slot)) {
            return 0;
        }

        set_page_free(slot);
        free_pages++;
        if (physical_address < range->scan) {
            range->scan = physical_address;
        }
        return 1;
    }
    return 0;
}

uint64_t pmm_total_pages(void) {
    return total_pages;
}

uint64_t pmm_free_pages(void) {
    return free_pages;
}

int pmm_is_ready(void) {
    return ready;
}

int pmm_is_limited(void) {
    return limited;
}

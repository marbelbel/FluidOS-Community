#ifndef VOS_PMM_H
#define VOS_PMM_H

#include <stdint.h>
#include "limine.h"

/* Boot-time page-frame allocator; callers must serialize alloc/free operations. */
/* Each successful allocation returns one zeroed 4096-byte page. */
int pmm_init(const struct limine_memmap_response *memory_map,
             const struct limine_hhdm_response *hhdm);
int pmm_alloc_page(uint64_t *physical_address, void **direct_map_address);
int pmm_free_page(uint64_t physical_address);
uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages(void);
int pmm_is_ready(void);
int pmm_is_limited(void);

#endif

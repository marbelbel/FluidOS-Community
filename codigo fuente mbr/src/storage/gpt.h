#ifndef VOS_GPT_H
#define VOS_GPT_H

#include <stdint.h>

#define GPT_PARTITION_LIMIT 32

enum gpt_status {
    GPT_OK = 0,
    GPT_INVALID = -1,
    GPT_NOT_FOUND = -2,
    GPT_IO_ERROR = -3,
    GPT_CORRUPT = -4,
    GPT_UNSUPPORTED = -5,
    GPT_TOO_MANY_PARTITIONS = -6,
    GPT_BUFFER_TOO_SMALL = -7
};

struct gpt_partition_info {
    uint8_t type_guid[16];
    uint8_t unique_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
};

/* Reads and validates the primary GPT. This routine never writes to the device.
 * It is not reentrant; serialize calls until the kernel has an I/O scheduler. */
int gpt_scan(uint32_t device_id, struct gpt_partition_info *partitions,
             uint32_t capacity, uint32_t *partition_count);

#endif

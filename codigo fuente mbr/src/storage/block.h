#ifndef VOS_BLOCK_H
#define VOS_BLOCK_H

#include <stdint.h>

#define BLOCK_DEVICE_LIMIT 8
#define BLOCK_DEVICE_NAME_SIZE 32
#define BLOCK_MAX_SECTOR_SIZE 4096
#define BLOCK_MAX_TRANSFER_BYTES (1024ULL * 1024ULL)

enum block_device_flag {
    /* A device is read-only unless its driver explicitly sets this flag. */
    BLOCK_DEVICE_WRITABLE = 1u << 0,
    BLOCK_DEVICE_REMOVABLE = 1u << 1
};

enum block_status {
    BLOCK_OK = 0,
    BLOCK_INVALID = -1,
    BLOCK_NOT_FOUND = -2,
    BLOCK_RANGE = -3,
    BLOCK_BUFFER_TOO_SMALL = -4,
    BLOCK_READ_ONLY = -5,
    BLOCK_IO_ERROR = -6,
    BLOCK_NO_SPACE = -7,
    BLOCK_UNSUPPORTED = -8
};

struct block_device_ops {
    int (*read)(void *context, uint64_t first_sector,
                uint32_t sector_count, void *buffer);
    int (*write)(void *context, uint64_t first_sector,
                 uint32_t sector_count, const void *buffer);
    int (*flush)(void *context);
};

struct block_device_info {
    char name[BLOCK_DEVICE_NAME_SIZE];
    uint64_t sector_count;
    uint32_t sector_size;
    uint32_t flags;
};

void block_init(void);
int block_register(const char *name, uint64_t sector_count,
                   uint32_t sector_size, uint32_t flags,
                   const struct block_device_ops *ops, void *context,
                   uint32_t *device_id);
uint32_t block_device_count(void);
int block_get_info(uint32_t device_id, struct block_device_info *info);
int block_read(uint32_t device_id, uint64_t first_sector,
               uint32_t sector_count, void *buffer, uint64_t buffer_bytes);
int block_write(uint32_t device_id, uint64_t first_sector,
                uint32_t sector_count, const void *buffer,
                uint64_t buffer_bytes);
int block_flush(uint32_t device_id);

#endif

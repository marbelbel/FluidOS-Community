#include "block.h"

struct registered_block_device {
    struct block_device_info info;
    struct block_device_ops ops;
    void *context;
    uint8_t registered;
};

static struct registered_block_device devices[BLOCK_DEVICE_LIMIT];
static uint32_t device_count;

static int copy_name(char *destination, const char *source) {
    if (destination == 0 || source == 0 || source[0] == '\0') return 0;
    for (uint32_t i = 0; i < BLOCK_DEVICE_NAME_SIZE; i++) {
        destination[i] = source[i];
        if (source[i] == '\0') return 1;
    }
    destination[0] = '\0';
    return 0;
}

static struct registered_block_device *find_device(uint32_t device_id) {
    if (device_id >= BLOCK_DEVICE_LIMIT || !devices[device_id].registered) {
        return 0;
    }
    return &devices[device_id];
}

void block_init(void) {
    for (uint32_t i = 0; i < BLOCK_DEVICE_LIMIT; i++) {
        devices[i].info.name[0] = '\0';
        devices[i].info.sector_count = 0;
        devices[i].info.sector_size = 0;
        devices[i].info.flags = 0;
        devices[i].ops.read = 0;
        devices[i].ops.write = 0;
        devices[i].ops.flush = 0;
        devices[i].context = 0;
        devices[i].registered = 0;
    }
    device_count = 0;
}

int block_register(const char *name, uint64_t sector_count,
                   uint32_t sector_size, uint32_t flags,
                   const struct block_device_ops *ops, void *context,
                   uint32_t *device_id) {
    if (name == 0 || sector_count == 0 || ops == 0 || ops->read == 0 ||
        (sector_size != 512 && sector_size != 1024 &&
         sector_size != 2048 && sector_size != 4096) ||
        (flags & ~(BLOCK_DEVICE_WRITABLE | BLOCK_DEVICE_REMOVABLE)) != 0) {
        return BLOCK_INVALID;
    }
    if ((flags & BLOCK_DEVICE_WRITABLE) != 0 &&
        (ops->write == 0 || ops->flush == 0)) {
        return BLOCK_INVALID;
    }
    if (device_count >= BLOCK_DEVICE_LIMIT) return BLOCK_NO_SPACE;

    uint32_t id = 0;
    while (id < BLOCK_DEVICE_LIMIT && devices[id].registered) id++;
    if (id == BLOCK_DEVICE_LIMIT) return BLOCK_NO_SPACE;

    struct registered_block_device *device = &devices[id];
    if (!copy_name(device->info.name, name)) return BLOCK_INVALID;
    device->info.sector_count = sector_count;
    device->info.sector_size = sector_size;
    device->info.flags = flags;
    device->ops = *ops;
    device->context = context;
    device->registered = 1;
    device_count++;
    if (device_id != 0) *device_id = id;
    return BLOCK_OK;
}

uint32_t block_device_count(void) {
    return device_count;
}

int block_get_info(uint32_t device_id, struct block_device_info *info) {
    struct registered_block_device *device = find_device(device_id);
    if (device == 0) return BLOCK_NOT_FOUND;
    if (info == 0) return BLOCK_INVALID;
    *info = device->info;
    return BLOCK_OK;
}

static int validate_transfer(struct registered_block_device *device,
                             uint64_t first_sector, uint32_t sector_count,
                             const void *buffer, uint64_t buffer_bytes,
                             uint64_t *transfer_bytes) {
    if (device == 0) return BLOCK_NOT_FOUND;
    if (sector_count == 0 || buffer == 0 || transfer_bytes == 0) {
        return BLOCK_INVALID;
    }
    if (first_sector >= device->info.sector_count ||
        (uint64_t)sector_count > device->info.sector_count - first_sector) {
        return BLOCK_RANGE;
    }
    if ((uint64_t)sector_count > UINT64_MAX / device->info.sector_size) {
        return BLOCK_RANGE;
    }
    *transfer_bytes = (uint64_t)sector_count * device->info.sector_size;
    if (*transfer_bytes > BLOCK_MAX_TRANSFER_BYTES) return BLOCK_RANGE;
    if (buffer_bytes < *transfer_bytes) return BLOCK_BUFFER_TOO_SMALL;
    return BLOCK_OK;
}

int block_read(uint32_t device_id, uint64_t first_sector,
               uint32_t sector_count, void *buffer, uint64_t buffer_bytes) {
    struct registered_block_device *device = find_device(device_id);
    uint64_t transfer_bytes = 0;
    int status = validate_transfer(device, first_sector, sector_count,
                                   buffer, buffer_bytes, &transfer_bytes);
    (void)transfer_bytes;
    if (status != BLOCK_OK) return status;
    return device->ops.read(device->context, first_sector, sector_count, buffer)
        ? BLOCK_OK : BLOCK_IO_ERROR;
}

int block_write(uint32_t device_id, uint64_t first_sector,
                uint32_t sector_count, const void *buffer,
                uint64_t buffer_bytes) {
    struct registered_block_device *device = find_device(device_id);
    if (device == 0) return BLOCK_NOT_FOUND;
    if ((device->info.flags & BLOCK_DEVICE_WRITABLE) == 0 ||
        device->ops.write == 0 || device->ops.flush == 0) {
        return BLOCK_READ_ONLY;
    }

    uint64_t transfer_bytes = 0;
    int status = validate_transfer(device, first_sector, sector_count,
                                   buffer, buffer_bytes, &transfer_bytes);
    (void)transfer_bytes;
    if (status != BLOCK_OK) return status;
    return device->ops.write(device->context, first_sector, sector_count, buffer)
        ? BLOCK_OK : BLOCK_IO_ERROR;
}

int block_flush(uint32_t device_id) {
    struct registered_block_device *device = find_device(device_id);
    if (device == 0) return BLOCK_NOT_FOUND;
    if ((device->info.flags & BLOCK_DEVICE_WRITABLE) == 0 ||
        device->ops.flush == 0) {
        return BLOCK_READ_ONLY;
    }
    return device->ops.flush(device->context) ? BLOCK_OK : BLOCK_IO_ERROR;
}

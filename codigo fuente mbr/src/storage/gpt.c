#include "gpt.h"

#include "block.h"

#define GPT_HEADER_MIN_SIZE 92u
#define GPT_ENTRY_MIN_SIZE 128u
#define GPT_ENTRY_MAX_SIZE 512u
#define GPT_ENTRY_COUNT_MAX 1024u

static uint8_t sector_buffer[BLOCK_MAX_SECTOR_SIZE];

static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static uint64_t read_le64(const uint8_t *bytes) {
    return (uint64_t)read_le32(bytes) |
           ((uint64_t)read_le32(bytes + 4) << 32);
}

static int guid_is_zero(const uint8_t guid[16]) {
    for (uint32_t i = 0; i < 16; i++) {
        if (guid[i] != 0) return 0;
    }
    return 1;
}

static int guid_equal(const uint8_t left[16], const uint8_t right[16]) {
    for (uint32_t i = 0; i < 16; i++) {
        if (left[i] != right[i]) return 0;
    }
    return 1;
}

static uint32_t crc32_begin(void) {
    return 0xffffffffu;
}

static uint32_t crc32_add(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (uint32_t bit = 0; bit < 8; bit++) {
        uint32_t mask = 0u - (crc & 1u);
        crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
    return crc;
}

static uint32_t crc32_end(uint32_t crc) {
    return crc ^ 0xffffffffu;
}

static int read_table_byte(uint32_t device_id, uint64_t array_lba,
                           uint32_t sector_size, uint64_t byte_offset,
                           uint64_t *cached_lba, uint8_t *value) {
    uint64_t lba = array_lba + byte_offset / sector_size;
    if (*cached_lba != lba) {
        if (block_read(device_id, lba, 1, sector_buffer,
                       sizeof(sector_buffer)) != BLOCK_OK) {
            return 0;
        }
        *cached_lba = lba;
    }
    *value = sector_buffer[byte_offset % sector_size];
    return 1;
}

static int partition_ranges_overlap(const struct gpt_partition_info *left,
                                    const struct gpt_partition_info *right) {
    return left->first_lba <= right->last_lba &&
           right->first_lba <= left->last_lba;
}

int gpt_scan(uint32_t device_id, struct gpt_partition_info *partitions,
             uint32_t capacity, uint32_t *partition_count) {
    if (partition_count == 0 || (capacity != 0 && partitions == 0)) {
        return GPT_INVALID;
    }
    *partition_count = 0;

    struct block_device_info device;
    if (block_get_info(device_id, &device) != BLOCK_OK) {
        return GPT_NOT_FOUND;
    }
    if (device.sector_size < 512 ||
        device.sector_size > BLOCK_MAX_SECTOR_SIZE) {
        return GPT_UNSUPPORTED;
    }
    if (device.sector_count <= 2 ||
        block_read(device_id, 1, 1, sector_buffer,
                   sizeof(sector_buffer)) != BLOCK_OK) {
        return device.sector_count <= 2 ? GPT_CORRUPT : GPT_IO_ERROR;
    }

    static const uint8_t signature[8] = {'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'};
    for (uint32_t i = 0; i < sizeof(signature); i++) {
        if (sector_buffer[i] != signature[i]) return GPT_NOT_FOUND;
    }

    uint32_t revision = read_le32(sector_buffer + 8);
    uint32_t header_size = read_le32(sector_buffer + 12);
    uint32_t expected_header_crc = read_le32(sector_buffer + 16);
    if (revision != 0x00010000u || header_size < GPT_HEADER_MIN_SIZE ||
        header_size > device.sector_size || read_le32(sector_buffer + 20) != 0) {
        return GPT_UNSUPPORTED;
    }

    uint32_t header_crc = crc32_begin();
    for (uint32_t i = 0; i < header_size; i++) {
        uint8_t byte = (i >= 16 && i < 20) ? 0 : sector_buffer[i];
        header_crc = crc32_add(header_crc, byte);
    }
    if (crc32_end(header_crc) != expected_header_crc) return GPT_CORRUPT;

    uint64_t current_lba = read_le64(sector_buffer + 24);
    uint64_t backup_lba = read_le64(sector_buffer + 32);
    uint64_t first_usable = read_le64(sector_buffer + 40);
    uint64_t last_usable = read_le64(sector_buffer + 48);
    uint64_t array_lba = read_le64(sector_buffer + 72);
    uint32_t entry_count = read_le32(sector_buffer + 80);
    uint32_t entry_size = read_le32(sector_buffer + 84);
    uint32_t expected_array_crc = read_le32(sector_buffer + 88);

    if (current_lba != 1 || backup_lba <= last_usable ||
        backup_lba >= device.sector_count || first_usable <= current_lba ||
        first_usable > last_usable || last_usable >= device.sector_count ||
        guid_is_zero(sector_buffer + 56) || entry_count == 0 ||
        entry_count > GPT_ENTRY_COUNT_MAX ||
        entry_size < GPT_ENTRY_MIN_SIZE || entry_size > GPT_ENTRY_MAX_SIZE ||
        (entry_size & 7u) != 0 || array_lba <= current_lba) {
        return GPT_CORRUPT;
    }

    uint64_t array_bytes = (uint64_t)entry_count * entry_size;
    uint64_t array_sectors = array_bytes / device.sector_size;
    if ((array_bytes % device.sector_size) != 0) array_sectors++;
    if (array_lba >= device.sector_count ||
        array_sectors > device.sector_count - array_lba ||
        array_lba + array_sectors > first_usable ||
        array_lba + array_sectors > backup_lba) {
        return GPT_CORRUPT;
    }

    struct gpt_partition_info staged[GPT_PARTITION_LIMIT];
    uint32_t staged_count = 0;
    uint8_t entry[GPT_ENTRY_MAX_SIZE];
    uint64_t cached_lba = UINT64_MAX;
    uint32_t array_crc = crc32_begin();

    for (uint64_t offset = 0; offset < array_bytes; offset++) {
        uint8_t byte;
        if (!read_table_byte(device_id, array_lba, device.sector_size,
                             offset, &cached_lba, &byte)) {
            return GPT_IO_ERROR;
        }
        array_crc = crc32_add(array_crc, byte);
        entry[offset % entry_size] = byte;

        if ((offset % entry_size) == entry_size - 1) {
            if (guid_is_zero(entry)) continue;
            if (staged_count == GPT_PARTITION_LIMIT) {
                return GPT_TOO_MANY_PARTITIONS;
            }

            struct gpt_partition_info candidate;
            for (uint32_t i = 0; i < 16; i++) {
                candidate.type_guid[i] = entry[i];
                candidate.unique_guid[i] = entry[16 + i];
            }
            candidate.first_lba = read_le64(entry + 32);
            candidate.last_lba = read_le64(entry + 40);
            candidate.attributes = read_le64(entry + 48);

            if (guid_is_zero(candidate.unique_guid) ||
                candidate.first_lba < first_usable ||
                candidate.first_lba > candidate.last_lba ||
                candidate.last_lba > last_usable) {
                return GPT_CORRUPT;
            }
            for (uint32_t i = 0; i < staged_count; i++) {
                if (partition_ranges_overlap(&candidate, &staged[i])) {
                    return GPT_CORRUPT;
                }
                if (guid_equal(candidate.unique_guid, staged[i].unique_guid)) {
                    return GPT_CORRUPT;
                }
            }
            staged[staged_count++] = candidate;
        }
    }

    if (crc32_end(array_crc) != expected_array_crc) return GPT_CORRUPT;
    *partition_count = staged_count;
    if (staged_count > capacity) return GPT_BUFFER_TOO_SMALL;
    for (uint32_t i = 0; i < staged_count; i++) {
        partitions[i] = staged[i];
    }
    return GPT_OK;
}

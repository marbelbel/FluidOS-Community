#include "fat32.h"

#include "block.h"

#define FAT32_EOC_MIN 0x0ffffff8u
#define FAT32_BAD_CLUSTER 0x0ffffff7u
#define FAT32_RESERVED_MIN 0x0ffffff0u
#define FAT32_CLUSTER_LIMIT 0x0ffffff5u
#define FAT32_CHAIN_HOP_LIMIT 65536u
#define FAT32_SCAN_SECTOR_LIMIT 65536u

static uint8_t io_sector[BLOCK_MAX_SECTOR_SIZE];
static uint8_t fat_cache[BLOCK_MAX_SECTOR_SIZE];
static uint64_t fat_cache_lba = UINT64_MAX;
static uint32_t fat_cache_device = UINT32_MAX;
static struct fat32_file staged_files[FAT32_DIRECTORY_LIMIT];

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int is_power_of_two(uint32_t value) {
    return value != 0 && (value & (value - 1u)) == 0;
}

static int read_relative_sector(struct fat32_volume *volume,
                                uint64_t relative_sector, uint8_t *buffer) {
    if (volume == 0 || !volume->mounted ||
        relative_sector >= volume->total_sectors || buffer == 0 ||
        relative_sector > UINT64_MAX - volume->partition_start_lba) {
        return 0;
    }
    uint64_t lba = volume->partition_start_lba + relative_sector;
    return block_read(volume->device_id, lba, 1, buffer,
                      BLOCK_MAX_SECTOR_SIZE) == BLOCK_OK;
}

static int cluster_is_valid(const struct fat32_volume *volume, uint32_t cluster) {
    return cluster >= 2 && (uint64_t)cluster < volume->cluster_count + 2u;
}

static int read_next_cluster(struct fat32_volume *volume, uint32_t cluster,
                             uint32_t *next_cluster, int *is_end) {
    if (volume == 0 || !volume->mounted || next_cluster == 0 || is_end == 0 ||
        !cluster_is_valid(volume, cluster)) {
        return FAT32_CORRUPT;
    }
    uint64_t fat_byte = (uint64_t)cluster * 4u;
    uint64_t fat_sector_offset = fat_byte / volume->sector_size;
    uint32_t byte_in_sector = (uint32_t)(fat_byte % volume->sector_size);
    if (fat_sector_offset >= volume->fat_size_sectors ||
        byte_in_sector + 4u > volume->sector_size) {
        return FAT32_CORRUPT;
    }
    uint64_t fat_lba = volume->fat_start_lba +
        (uint64_t)volume->active_fat * volume->fat_size_sectors +
        fat_sector_offset;
    if (fat_cache_device != volume->device_id || fat_cache_lba != fat_lba) {
        uint64_t relative = fat_lba - volume->partition_start_lba;
        if (!read_relative_sector(volume, relative, fat_cache)) {
            return FAT32_IO_ERROR;
        }
        fat_cache_device = volume->device_id;
        fat_cache_lba = fat_lba;
    }

    uint32_t value = read_le32(fat_cache + byte_in_sector) & 0x0fffffffu;
    if (value >= FAT32_EOC_MIN) {
        *is_end = 1;
        *next_cluster = 0;
        return FAT32_OK;
    }
    if (value == 0 || value == 1 || value == FAT32_BAD_CLUSTER ||
        value >= FAT32_RESERVED_MIN || !cluster_is_valid(volume, value)) {
        return FAT32_CORRUPT;
    }
    *is_end = 0;
    *next_cluster = value;
    return FAT32_OK;
}

int fat32_mount(struct fat32_volume *volume, uint32_t device_id,
                uint64_t partition_start_lba,
                uint64_t partition_sector_count) {
    if (volume == 0 || partition_sector_count == 0) return FAT32_INVALID;
    volume->mounted = 0;

    struct block_device_info device;
    if (block_get_info(device_id, &device) != BLOCK_OK) return FAT32_INVALID;
    if (device.sector_size < 512 || device.sector_size > BLOCK_MAX_SECTOR_SIZE) {
        return FAT32_UNSUPPORTED;
    }
    if (partition_start_lba >= device.sector_count ||
        partition_sector_count > device.sector_count - partition_start_lba) {
        return FAT32_INVALID;
    }
    if (block_read(device_id, partition_start_lba, 1, io_sector,
                   sizeof(io_sector)) != BLOCK_OK) {
        return FAT32_IO_ERROR;
    }
    if (io_sector[510] != 0x55 || io_sector[511] != 0xaa) {
        return FAT32_NOT_FAT32;
    }

    uint32_t bytes_per_sector = read_le16(io_sector + 11);
    uint32_t sectors_per_cluster = io_sector[13];
    uint32_t reserved_sectors = read_le16(io_sector + 14);
    uint32_t fat_count = io_sector[16];
    uint32_t root_entry_count = read_le16(io_sector + 17);
    uint32_t total_sectors_16 = read_le16(io_sector + 19);
    uint32_t fat_size_16 = read_le16(io_sector + 22);
    uint32_t total_sectors = read_le32(io_sector + 32);
    uint32_t fat_size_32 = read_le32(io_sector + 36);
    uint16_t extended_flags = read_le16(io_sector + 40);
    uint16_t filesystem_version = read_le16(io_sector + 42);
    uint32_t root_cluster = read_le32(io_sector + 44) & 0x0fffffffu;

    if (bytes_per_sector != device.sector_size || !is_power_of_two(sectors_per_cluster) ||
        sectors_per_cluster > 128 || reserved_sectors == 0 ||
        (fat_count != 1 && fat_count != 2) || root_entry_count != 0 ||
        total_sectors_16 != 0 || fat_size_16 != 0 || total_sectors == 0 ||
        fat_size_32 == 0 || filesystem_version != 0) {
        return FAT32_NOT_FAT32;
    }
    uint32_t active_fat = (extended_flags & 0x0080u) != 0 ?
        (extended_flags & 0x000fu) : 0;
    if (active_fat >= fat_count) return FAT32_CORRUPT;
    if ((uint64_t)total_sectors > partition_sector_count) return FAT32_CORRUPT;

    uint64_t fat_area_sectors = (uint64_t)fat_count * fat_size_32;
    if ((uint64_t)reserved_sectors > UINT64_MAX - fat_area_sectors) {
        return FAT32_CORRUPT;
    }
    fat_area_sectors += reserved_sectors;
    if (fat_area_sectors >= total_sectors) return FAT32_CORRUPT;
    uint64_t data_sectors = (uint64_t)total_sectors - fat_area_sectors;
    uint64_t cluster_count = data_sectors / sectors_per_cluster;
    if (cluster_count == 0 || cluster_count >= FAT32_CLUSTER_LIMIT ||
        (uint64_t)root_cluster < 2 ||
        (uint64_t)root_cluster >= cluster_count + 2u) {
        return FAT32_CORRUPT;
    }
    uint64_t fat_entries = ((uint64_t)fat_size_32 * bytes_per_sector) / 4u;
    if (fat_entries < cluster_count + 2u) return FAT32_CORRUPT;

    struct fat32_volume staged;
    staged.device_id = device_id;
    staged.sector_size = bytes_per_sector;
    staged.sectors_per_cluster = sectors_per_cluster;
    staged.reserved_sectors = reserved_sectors;
    staged.fat_count = fat_count;
    staged.active_fat = active_fat;
    staged.fat_size_sectors = fat_size_32;
    staged.root_cluster = root_cluster;
    staged.partition_start_lba = partition_start_lba;
    staged.partition_sector_count = partition_sector_count;
    staged.total_sectors = total_sectors;
    staged.fat_start_lba = partition_start_lba + reserved_sectors;
    /* fat_area_sectors ya incluye los sectores reservados. */
    staged.data_start_lba = partition_start_lba + fat_area_sectors;
    staged.cluster_count = cluster_count;
    staged.mounted = 1;
    *volume = staged;
    fat_cache_lba = UINT64_MAX;
    fat_cache_device = UINT32_MAX;
    return FAT32_OK;
}

int fat32_root_list(struct fat32_volume *volume, struct fat32_file *files,
                    uint32_t capacity, uint32_t *file_count) {
    if (file_count == 0 || (capacity != 0 && files == 0)) return FAT32_INVALID;
    *file_count = 0;
    if (volume == 0 || !volume->mounted) return FAT32_INVALID;

    uint32_t staged_count = 0;
    uint32_t cluster = volume->root_cluster;
    uint32_t sectors_scanned = 0;
    for (uint64_t cluster_hops = 0; cluster_hops < volume->cluster_count;
         cluster_hops++) {
        if (!cluster_is_valid(volume, cluster)) return FAT32_CORRUPT;
        uint64_t cluster_offset = (uint64_t)(cluster - 2u) *
                                  volume->sectors_per_cluster;
        uint64_t data_relative = volume->data_start_lba -
                                 volume->partition_start_lba;
        if (data_relative > volume->total_sectors ||
            cluster_offset > volume->total_sectors - data_relative) {
            return FAT32_CORRUPT;
        }
        uint64_t cluster_lba = volume->data_start_lba + cluster_offset;
        for (uint32_t sector = 0; sector < volume->sectors_per_cluster; sector++) {
            if (sectors_scanned++ >= FAT32_SCAN_SECTOR_LIMIT) {
                return FAT32_TOO_MANY_ENTRIES;
            }
            uint64_t relative = cluster_lba + sector - volume->partition_start_lba;
            if (!read_relative_sector(volume, relative, io_sector)) {
                return FAT32_IO_ERROR;
            }
            for (uint32_t entry_offset = 0;
                 entry_offset + 32u <= volume->sector_size; entry_offset += 32u) {
                const uint8_t *entry = io_sector + entry_offset;
                if (entry[0] == 0) {
                    *file_count = staged_count;
                    if (staged_count > capacity) return FAT32_BUFFER_TOO_SMALL;
                    for (uint32_t i = 0; i < staged_count; i++) files[i] = staged_files[i];
                    return FAT32_OK;
                }
                if (entry[0] == 0xe5 || entry[11] == 0x0f ||
                    (entry[11] & 0x08u) != 0) {
                    continue;
                }
                if (staged_count >= FAT32_DIRECTORY_LIMIT) {
                    return FAT32_TOO_MANY_ENTRIES;
                }
                struct fat32_file candidate;
                for (uint32_t i = 0; i < FAT32_NAME_BYTES; i++) {
                    candidate.short_name[i] = entry[i];
                }
                candidate.attributes = entry[11];
                candidate.first_cluster =
                    ((uint32_t)read_le16(entry + 20) << 16) |
                    read_le16(entry + 26);
                candidate.size = read_le32(entry + 28);
                if ((candidate.attributes & 0x10u) != 0 || candidate.size != 0) {
                    if (!cluster_is_valid(volume, candidate.first_cluster)) {
                        return FAT32_CORRUPT;
                    }
                }
                staged_files[staged_count++] = candidate;
            }
        }
        uint32_t next;
        int end;
        int status = read_next_cluster(volume, cluster, &next, &end);
        if (status != FAT32_OK) return status;
        if (end) {
            *file_count = staged_count;
            if (staged_count > capacity) return FAT32_BUFFER_TOO_SMALL;
            for (uint32_t i = 0; i < staged_count; i++) files[i] = staged_files[i];
            return FAT32_OK;
        }
        cluster = next;
    }
    return FAT32_CORRUPT;
}

int fat32_read_file(struct fat32_volume *volume,
                    const struct fat32_file *file, uint64_t offset,
                    void *buffer, uint32_t requested_bytes,
                    uint32_t *bytes_read) {
    if (bytes_read == 0 || file == 0 ||
        (requested_bytes != 0 && buffer == 0)) return FAT32_INVALID;
    *bytes_read = 0;
    if (volume == 0 || !volume->mounted || (file->attributes & 0x10u) != 0) {
        return FAT32_INVALID;
    }
    if (requested_bytes > FAT32_MAX_READ_BYTES) return FAT32_UNSUPPORTED;
    if (offset >= file->size || requested_bytes == 0) return FAT32_OK;
    if (!cluster_is_valid(volume, file->first_cluster)) return FAT32_CORRUPT;

    uint64_t cluster_bytes = (uint64_t)volume->sector_size *
                             volume->sectors_per_cluster;
    uint64_t maximum_file_size = volume->cluster_count * cluster_bytes;
    if (file->size > maximum_file_size) return FAT32_CORRUPT;
    uint64_t remaining_file_bytes = (uint64_t)file->size - offset;
    uint32_t remaining = remaining_file_bytes < requested_bytes ?
        (uint32_t)remaining_file_bytes : requested_bytes;
    uint64_t cluster_index = offset / cluster_bytes;
    if (cluster_index >= volume->cluster_count) return FAT32_CORRUPT;
    if (cluster_index > FAT32_CHAIN_HOP_LIMIT) return FAT32_UNSUPPORTED;

    uint32_t cluster = file->first_cluster;
    for (uint64_t i = 0; i < cluster_index; i++) {
        uint32_t next;
        int end;
        int status = read_next_cluster(volume, cluster, &next, &end);
        if (status != FAT32_OK) return status;
        if (end) return FAT32_CORRUPT;
        cluster = next;
    }

    uint8_t *destination = (uint8_t *)buffer;
    uint32_t copied = 0;
    uint64_t file_offset = offset;
    while (copied < remaining) {
        if (!cluster_is_valid(volume, cluster)) return FAT32_CORRUPT;
        uint64_t within_cluster = file_offset % cluster_bytes;
        uint32_t sector_in_cluster = (uint32_t)(within_cluster / volume->sector_size);
        uint32_t byte_in_sector = (uint32_t)(within_cluster % volume->sector_size);
        uint32_t chunk = volume->sector_size - byte_in_sector;
        uint64_t cluster_remaining = cluster_bytes - within_cluster;
        if (chunk > cluster_remaining) chunk = (uint32_t)cluster_remaining;
        if (chunk > remaining - copied) chunk = remaining - copied;

        uint64_t cluster_offset = (uint64_t)(cluster - 2u) *
                                  volume->sectors_per_cluster;
        uint64_t relative = volume->data_start_lba + cluster_offset +
                            sector_in_cluster - volume->partition_start_lba;
        if (!read_relative_sector(volume, relative, io_sector)) return FAT32_IO_ERROR;
        for (uint32_t i = 0; i < chunk; i++) {
            destination[copied + i] = io_sector[byte_in_sector + i];
        }
        copied += chunk;
        file_offset += chunk;

        if (copied < remaining && file_offset % cluster_bytes == 0) {
            uint32_t next;
            int end;
            int status = read_next_cluster(volume, cluster, &next, &end);
            if (status != FAT32_OK) return status;
            if (end) return FAT32_CORRUPT;
            cluster = next;
        }
    }
    *bytes_read = copied;
    return FAT32_OK;
}

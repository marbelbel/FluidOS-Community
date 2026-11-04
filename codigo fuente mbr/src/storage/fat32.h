#ifndef VOS_FAT32_H
#define VOS_FAT32_H

#include <stdint.h>

#define FAT32_NAME_BYTES 11u
#define FAT32_DIRECTORY_LIMIT 256u
#define FAT32_MAX_READ_BYTES (1024u * 1024u)

enum fat32_status {
    FAT32_OK = 0,
    FAT32_INVALID = -1,
    FAT32_IO_ERROR = -2,
    FAT32_NOT_FAT32 = -3,
    FAT32_CORRUPT = -4,
    FAT32_UNSUPPORTED = -5,
    FAT32_BUFFER_TOO_SMALL = -6,
    FAT32_TOO_MANY_ENTRIES = -7,
    FAT32_READ_ONLY = -8
};

struct fat32_volume {
    uint32_t device_id;
    uint32_t sector_size;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t active_fat;
    uint32_t fat_size_sectors;
    uint32_t root_cluster;
    uint64_t partition_start_lba;
    uint64_t partition_sector_count;
    uint64_t total_sectors;
    uint64_t fat_start_lba;
    uint64_t data_start_lba;
    uint64_t cluster_count;
    uint8_t mounted;
};

struct fat32_file {
    uint8_t short_name[FAT32_NAME_BYTES];
    uint8_t attributes;
    uint32_t first_cluster;
    uint32_t size;
};

/* Mounts a partition read-only. Calls are serialized; this early driver has
 * one bounded sector buffer and intentionally supports one operation at once. */
int fat32_mount(struct fat32_volume *volume, uint32_t device_id,
                uint64_t partition_start_lba,
                uint64_t partition_sector_count);

/* Lists regular files and directories from the FAT32 root (8.3 names only). */
int fat32_root_list(struct fat32_volume *volume, struct fat32_file *files,
                    uint32_t capacity, uint32_t *file_count);

/* Reads at most FAT32_MAX_READ_BYTES from one root entry. No write API exists. */
int fat32_read_file(struct fat32_volume *volume,
                    const struct fat32_file *file, uint64_t offset,
                    void *buffer, uint32_t requested_bytes,
                    uint32_t *bytes_read);

#endif

#include <stdint.h>

#include "block.h"
#include "gpt.h"

#define TEST_SECTOR_SIZE 512u
#define TEST_SECTOR_COUNT 128u
#define TEST_DISK_BYTES (TEST_SECTOR_SIZE * TEST_SECTOR_COUNT)
#define TEST_ENTRY_SIZE 128u
#define TEST_ENTRY_COUNT 4u
#define TEST_ENTRY_ARRAY_LBA 2u

static uint8_t test_disk[TEST_DISK_BYTES];

static void put_le32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void put_le64(uint8_t *bytes, uint64_t value) {
    put_le32(bytes, (uint32_t)value);
    put_le32(bytes + 4, (uint32_t)(value >> 32));
}

static uint32_t crc32(const uint8_t *bytes, uint32_t length) {
    uint32_t crc = 0xffffffffu;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint32_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return crc ^ 0xffffffffu;
}

static void set_partition(uint32_t index, uint8_t unique_id,
                          uint64_t first_lba, uint64_t last_lba) {
    uint8_t *entry = test_disk + TEST_ENTRY_ARRAY_LBA * TEST_SECTOR_SIZE +
                     index * TEST_ENTRY_SIZE;
    entry[0] = 0xa1;
    entry[16] = unique_id;
    put_le64(entry + 32, first_lba);
    put_le64(entry + 40, last_lba);
}

static void finish_gpt(void) {
    uint8_t *header = test_disk + TEST_SECTOR_SIZE;
    uint8_t *entries = test_disk + TEST_ENTRY_ARRAY_LBA * TEST_SECTOR_SIZE;
    put_le32(header + 88, crc32(entries, TEST_ENTRY_COUNT * TEST_ENTRY_SIZE));
    put_le32(header + 16, 0);
    put_le32(header + 16, crc32(header, 92));
}

static void make_valid_disk(void) {
    for (uint32_t i = 0; i < TEST_DISK_BYTES; i++) test_disk[i] = 0;

    uint8_t *header = test_disk + TEST_SECTOR_SIZE;
    static const uint8_t signature[8] = {'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'};
    for (uint32_t i = 0; i < sizeof(signature); i++) header[i] = signature[i];
    put_le32(header + 8, 0x00010000u);
    put_le32(header + 12, 92);
    put_le64(header + 24, 1);
    put_le64(header + 32, TEST_SECTOR_COUNT - 1);
    put_le64(header + 40, 34);
    put_le64(header + 48, 100);
    header[56] = 1;
    put_le64(header + 72, TEST_ENTRY_ARRAY_LBA);
    put_le32(header + 80, TEST_ENTRY_COUNT);
    put_le32(header + 84, TEST_ENTRY_SIZE);

    set_partition(0, 1, 34, 40);
    finish_gpt();
}

static int memory_disk_read(void *context, uint64_t first_sector,
                            uint32_t sector_count, void *buffer) {
    const uint8_t *disk = (const uint8_t *)context;
    if (disk == 0 || buffer == 0 || first_sector >= TEST_SECTOR_COUNT ||
        sector_count > TEST_SECTOR_COUNT - first_sector) {
        return 0;
    }
    uint64_t byte_start = first_sector * TEST_SECTOR_SIZE;
    uint64_t byte_count = (uint64_t)sector_count * TEST_SECTOR_SIZE;
    uint8_t *output = (uint8_t *)buffer;
    for (uint64_t i = 0; i < byte_count; i++) {
        output[i] = disk[byte_start + i];
    }
    return 1;
}

static int register_memory_disk(void) {
    struct block_device_ops ops = {
        .read = memory_disk_read,
        .write = 0,
        .flush = 0
    };
    block_init();
    return block_register("test-memory-disk", TEST_SECTOR_COUNT,
                          TEST_SECTOR_SIZE, 0, &ops, test_disk, 0) == BLOCK_OK;
}

static int run_valid_table_case(void) {
    make_valid_disk();
    if (!register_memory_disk()) return 0;

    struct gpt_partition_info partitions[GPT_PARTITION_LIMIT];
    uint32_t count = 0;
    if (gpt_scan(0, partitions, GPT_PARTITION_LIMIT, &count) != GPT_OK) return 0;
    return count == 1 && partitions[0].first_lba == 34 &&
           partitions[0].last_lba == 40 && partitions[0].unique_guid[0] == 1;
}

static int run_small_buffer_case(void) {
    make_valid_disk();
    if (!register_memory_disk()) return 0;
    uint32_t count = 0;
    int status = gpt_scan(0, 0, 0, &count);
    return status == GPT_BUFFER_TOO_SMALL && count == 1;
}

static int run_read_only_case(void) {
    make_valid_disk();
    if (!register_memory_disk()) return 0;
    uint8_t sector[TEST_SECTOR_SIZE];
    return block_write(0, 0, 1, sector, sizeof(sector)) == BLOCK_READ_ONLY;
}

static int run_bad_header_crc_case(void) {
    make_valid_disk();
    test_disk[TEST_SECTOR_SIZE + 16] ^= 1;
    if (!register_memory_disk()) return 0;
    struct gpt_partition_info partition[GPT_PARTITION_LIMIT];
    uint32_t count = 0;
    return gpt_scan(0, partition, GPT_PARTITION_LIMIT, &count) == GPT_CORRUPT;
}

static int run_bad_table_crc_case(void) {
    make_valid_disk();
    uint8_t *header = test_disk + TEST_SECTOR_SIZE;
    put_le32(header + 88, 0);
    put_le32(header + 16, 0);
    put_le32(header + 16, crc32(header, 92));
    if (!register_memory_disk()) return 0;
    struct gpt_partition_info partition[GPT_PARTITION_LIMIT];
    uint32_t count = 0;
    return gpt_scan(0, partition, GPT_PARTITION_LIMIT, &count) == GPT_CORRUPT;
}

static int run_bad_partition_range_case(void) {
    make_valid_disk();
    uint8_t *entry = test_disk + TEST_ENTRY_ARRAY_LBA * TEST_SECTOR_SIZE;
    put_le64(entry + 40, 101);
    finish_gpt();
    if (!register_memory_disk()) return 0;
    struct gpt_partition_info partition[GPT_PARTITION_LIMIT];
    uint32_t count = 0;
    return gpt_scan(0, partition, GPT_PARTITION_LIMIT, &count) == GPT_CORRUPT;
}

static int run_overlap_case(void) {
    make_valid_disk();
    set_partition(1, 2, 40, 45);
    finish_gpt();
    if (!register_memory_disk()) return 0;
    struct gpt_partition_info partition[GPT_PARTITION_LIMIT];
    uint32_t count = 0;
    return gpt_scan(0, partition, GPT_PARTITION_LIMIT, &count) == GPT_CORRUPT;
}

int main(void) {
    if (!run_valid_table_case()) return 1;
    if (!run_small_buffer_case()) return 2;
    if (!run_read_only_case()) return 3;
    if (!run_bad_header_crc_case()) return 4;
    if (!run_bad_table_crc_case()) return 5;
    if (!run_bad_partition_range_case()) return 6;
    if (!run_overlap_case()) return 7;
    return 0;
}

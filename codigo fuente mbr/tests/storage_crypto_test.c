#include <stdint.h>

#include "block.h"
#include "fat32.h"
#include "package_crypto.h"
#include "package_verify.h"
#include "sha256.h"

#define DISK_SECTOR_SIZE 512u
#define DISK_SECTOR_COUNT 128u
#define DISK_PARTITION_START 4u
#define DISK_PARTITION_SECTORS 100u
#define PACKAGE_PAYLOAD_SIZE 3u
#define PACKAGE_TOTAL_SIZE (PACKAGE_HEADER_SIZE + PACKAGE_PAYLOAD_SIZE)

static uint8_t test_disk[DISK_SECTOR_SIZE * DISK_SECTOR_COUNT];
static uint8_t test_package[PACKAGE_TOTAL_SIZE];
static int test_signature_mode;

static void put_le16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

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

static int memory_disk_read(void *context, uint64_t first_sector,
                            uint32_t sector_count, void *buffer) {
    const uint8_t *disk = (const uint8_t *)context;
    if (disk == 0 || buffer == 0 || first_sector >= DISK_SECTOR_COUNT ||
        sector_count > DISK_SECTOR_COUNT - first_sector) {
        return 0;
    }
    uint64_t start = first_sector * DISK_SECTOR_SIZE;
    uint64_t length = (uint64_t)sector_count * DISK_SECTOR_SIZE;
    uint8_t *output = (uint8_t *)buffer;
    for (uint64_t i = 0; i < length; i++) output[i] = disk[start + i];
    return 1;
}

static int register_memory_disk(void) {
    struct block_device_ops operations = {
        .read = memory_disk_read,
        .write = 0,
        .flush = 0
    };
    block_init();
    return block_register("test-storage", DISK_SECTOR_COUNT, DISK_SECTOR_SIZE,
                          0, &operations, test_disk, 0) == BLOCK_OK;
}

static int test_sha256(void) {
    static const uint8_t expected_empty[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    static const uint8_t expected_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    uint8_t digest[32];
    if (!sha256_digest(0, 0, digest)) return 0;
    for (uint32_t i = 0; i < sizeof(digest); i++) {
        if (digest[i] != expected_empty[i]) return 0;
    }

    struct sha256_context context;
    sha256_init(&context);
    if (!sha256_update(&context, "a", 1) ||
        !sha256_update(&context, "bc", 2) ||
        !sha256_final(&context, digest)) {
        return 0;
    }
    for (uint32_t i = 0; i < sizeof(digest); i++) {
        if (digest[i] != expected_abc[i]) return 0;
    }
    return !sha256_update(&context, "x", 1);
}

static int package_memory_read(void *context, uint64_t offset,
                               void *buffer, uint32_t bytes) {
    const uint8_t *package = (const uint8_t *)context;
    if (package == 0 || buffer == 0 || offset > PACKAGE_TOTAL_SIZE ||
        bytes > PACKAGE_TOTAL_SIZE - offset) {
        return 0;
    }
    uint8_t *output = (uint8_t *)buffer;
    for (uint32_t i = 0; i < bytes; i++) output[i] = package[offset + i];
    return 1;
}

int package_crypto_verify_trusted(
    const uint8_t key_id[PACKAGE_KEY_ID_SIZE], const uint8_t *message,
    uint32_t message_size, const uint8_t signature[PACKAGE_SIGNATURE_SIZE]) {
    static const uint8_t domain[16] = {
        'V', 'O', 'S', '-', 'P', 'K', 'G', '-',
        'S', 'I', 'G', 'N', '-', 'V', '1', 0
    };
    if (test_signature_mode != PACKAGE_CRYPTO_VALID_SIGNATURE) {
        return test_signature_mode;
    }
    if (key_id[0] != 0x42 || message_size != 88 || signature[0] != 0xa5)
        return PACKAGE_CRYPTO_INVALID_SIGNATURE;
    for (uint32_t i = 0; i < sizeof(domain); i++) {
        if (message[i] != domain[i]) return PACKAGE_CRYPTO_INVALID_SIGNATURE;
    }
    return PACKAGE_CRYPTO_VALID_SIGNATURE;
}

static void make_test_package(void) {
    for (uint32_t i = 0; i < sizeof(test_package); i++) test_package[i] = 0;
    static const uint8_t magic[8] = {'V', 'O', 'S', 'P', 'K', 'G', '1', 0};
    for (uint32_t i = 0; i < sizeof(magic); i++) test_package[i] = magic[i];
    put_le16(test_package + 8, 1);
    put_le16(test_package + 10, 1);
    put_le32(test_package + 12, PACKAGE_HEADER_SIZE);
    put_le64(test_package + 16, PACKAGE_PAYLOAD_SIZE);
    test_package[24] = 0x42;
    test_package[72] = 0xa5;
    test_package[PACKAGE_HEADER_SIZE] = 'V';
    test_package[PACKAGE_HEADER_SIZE + 1] = 'O';
    test_package[PACKAGE_HEADER_SIZE + 2] = 'S';
    (void)sha256_digest(test_package + PACKAGE_HEADER_SIZE,
                        PACKAGE_PAYLOAD_SIZE, test_package + 40);
}

static int test_package_verifier(void) {
    make_test_package();
    struct package_info info;
    test_signature_mode = PACKAGE_CRYPTO_NO_TRUSTED_KEY;
    if (package_verify(package_memory_read, test_package, sizeof(test_package),
                       &info) != PACKAGE_NO_TRUSTED_VERIFIER) {
        return 0;
    }
    test_signature_mode = PACKAGE_CRYPTO_VALID_SIGNATURE;
    if (package_verify(package_memory_read, test_package, sizeof(test_package),
                       &info) != PACKAGE_OK ||
        info.payload_size != PACKAGE_PAYLOAD_SIZE || info.key_id[0] != 0x42) {
        return 0;
    }

    test_signature_mode = PACKAGE_CRYPTO_INVALID_SIGNATURE;
    if (package_verify(package_memory_read, test_package, sizeof(test_package),
                       &info) !=
        PACKAGE_BAD_SIGNATURE) {
        return 0;
    }
    test_signature_mode = PACKAGE_CRYPTO_VALID_SIGNATURE;
    test_package[PACKAGE_HEADER_SIZE] ^= 1;
    int status = package_verify(package_memory_read, test_package,
                                sizeof(test_package), &info);
    test_signature_mode = PACKAGE_CRYPTO_NO_TRUSTED_KEY;
    return status == PACKAGE_BAD_HASH;
}

static void make_test_fat32(void) {
    for (uint32_t i = 0; i < sizeof(test_disk); i++) test_disk[i] = 0;
    uint8_t *boot = test_disk + DISK_PARTITION_START * DISK_SECTOR_SIZE;
    put_le16(boot + 11, DISK_SECTOR_SIZE);
    boot[13] = 1;
    put_le16(boot + 14, 1);
    boot[16] = 1;
    put_le32(boot + 32, DISK_PARTITION_SECTORS);
    put_le32(boot + 36, 1);
    put_le16(boot + 40, 0);
    put_le16(boot + 42, 0);
    put_le32(boot + 44, 2);
    boot[510] = 0x55;
    boot[511] = 0xaa;

    uint8_t *fat = test_disk + (DISK_PARTITION_START + 1u) * DISK_SECTOR_SIZE;
    put_le32(fat + 2u * 4u, 0x0fffffffu);
    put_le32(fat + 3u * 4u, 4u);
    put_le32(fat + 4u * 4u, 0x0fffffffu);

    uint8_t *root = test_disk + (DISK_PARTITION_START + 2u) * DISK_SECTOR_SIZE;
    static const uint8_t short_name[11] = {
        'H', 'E', 'L', 'L', 'O', ' ', ' ', ' ', 'T', 'X', 'T'
    };
    for (uint32_t i = 0; i < sizeof(short_name); i++) root[i] = short_name[i];
    root[11] = 0x20;
    put_le16(root + 26, 3);
    put_le32(root + 28, 600);

    uint8_t *first_data = test_disk + (DISK_PARTITION_START + 3u) * DISK_SECTOR_SIZE;
    uint8_t *second_data = test_disk + (DISK_PARTITION_START + 4u) * DISK_SECTOR_SIZE;
    for (uint32_t i = 0; i < DISK_SECTOR_SIZE; i++) first_data[i] = 'a';
    for (uint32_t i = 0; i < DISK_SECTOR_SIZE; i++) second_data[i] = 'b';
}

static int test_fat32_read_only_files(void) {
    make_test_fat32();
    if (!register_memory_disk()) return 0;
    struct fat32_volume volume;
    if (fat32_mount(&volume, 0, DISK_PARTITION_START,
                    DISK_PARTITION_SECTORS) != FAT32_OK) {
        return 0;
    }
    struct fat32_file file;
    uint32_t count = 0;
    if (fat32_root_list(&volume, &file, 1, &count) != FAT32_OK || count != 1 ||
        file.size != 600 || file.first_cluster != 3) {
        return 0;
    }
    uint8_t output[100];
    uint32_t bytes_read = 0;
    if (fat32_read_file(&volume, &file, 500, output, sizeof(output),
                        &bytes_read) != FAT32_OK || bytes_read != sizeof(output)) {
        return 0;
    }
    for (uint32_t i = 0; i < 12; i++) if (output[i] != 'a') return 0;
    for (uint32_t i = 12; i < sizeof(output); i++) if (output[i] != 'b') return 0;

    uint8_t *fat = test_disk + (DISK_PARTITION_START + 1u) * DISK_SECTOR_SIZE;
    put_le32(fat + 3u * 4u, 0x0fffffffu);
    if (fat32_mount(&volume, 0, DISK_PARTITION_START,
                    DISK_PARTITION_SECTORS) != FAT32_OK) {
        return 0;
    }
    if (fat32_root_list(&volume, &file, 1, &count) != FAT32_OK || count != 1) {
        return 0;
    }
    if (fat32_read_file(&volume, &file, 500, output, sizeof(output),
                        &bytes_read) != FAT32_CORRUPT) {
        return 0;
    }
    return fat32_mount(&volume, 0, 120, 20) == FAT32_INVALID;
}

int main(void) {
    if (!test_sha256()) return 1;
    if (!test_package_verifier()) return 2;
    if (!test_fat32_read_only_files()) return 3;
    return 0;
}

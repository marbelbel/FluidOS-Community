#include "package_verify.h"

#include "package_crypto.h"
#include "sha256.h"

#define PACKAGE_SIGNED_PREFIX_SIZE 72u
#define PACKAGE_SIGNED_MESSAGE_SIZE 88u
#define PACKAGE_HASH_CHUNK_SIZE 4096u

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint64_t read_le64(const uint8_t *bytes) {
    return (uint64_t)read_le32(bytes) | ((uint64_t)read_le32(bytes + 4) << 32);
}

static int constant_time_equal(const uint8_t *left, const uint8_t *right,
                               uint32_t length) {
    uint8_t difference = 0;
    for (uint32_t i = 0; i < length; i++) difference |= left[i] ^ right[i];
    return difference == 0;
}

int package_verify(package_read_at_fn read_at, void *read_context,
                   uint64_t package_size, struct package_info *verified_info) {
    if (read_at == 0 || verified_info == 0) return PACKAGE_INVALID;
    if (package_size < PACKAGE_HEADER_SIZE) return PACKAGE_BAD_FORMAT;

    uint8_t header[PACKAGE_HEADER_SIZE];
    if (!read_at(read_context, 0, header, sizeof(header))) return PACKAGE_IO_ERROR;
    static const uint8_t expected_magic[8] = {
        'V', 'O', 'S', 'P', 'K', 'G', '1', 0
    };
    for (uint32_t i = 0; i < sizeof(expected_magic); i++) {
        if (header[i] != expected_magic[i]) return PACKAGE_BAD_FORMAT;
    }

    uint16_t version = read_le16(header + 8);
    uint16_t architecture = read_le16(header + 10);
    uint32_t header_size = read_le32(header + 12);
    uint64_t payload_size = read_le64(header + 16);
    if (version != 1 || architecture != PACKAGE_ARCH_X86_64 ||
        header_size != PACKAGE_HEADER_SIZE || payload_size == 0) {
        return PACKAGE_BAD_FORMAT;
    }
    if (payload_size > PACKAGE_MAX_PAYLOAD_BYTES) return PACKAGE_TOO_LARGE;
    if (payload_size > UINT64_MAX - PACKAGE_HEADER_SIZE ||
        PACKAGE_HEADER_SIZE + payload_size != package_size) {
        return PACKAGE_BAD_FORMAT;
    }

    uint8_t signed_message[PACKAGE_SIGNED_MESSAGE_SIZE];
    static const uint8_t domain[16] = {
        'V', 'O', 'S', '-', 'P', 'K', 'G', '-',
        'S', 'I', 'G', 'N', '-', 'V', '1', 0
    };
    for (uint32_t i = 0; i < sizeof(domain); i++) signed_message[i] = domain[i];
    for (uint32_t i = 0; i < PACKAGE_SIGNED_PREFIX_SIZE; i++) {
        signed_message[16u + i] = header[i];
    }

    int signature_status = package_crypto_verify_trusted(
        header + 24, signed_message, sizeof(signed_message), header + 72);
    if (signature_status == PACKAGE_CRYPTO_NO_TRUSTED_KEY) {
        return PACKAGE_NO_TRUSTED_VERIFIER;
    }
    if (signature_status != PACKAGE_CRYPTO_VALID_SIGNATURE) {
        return PACKAGE_BAD_SIGNATURE;
    }

    struct sha256_context hash_context;
    sha256_init(&hash_context);
    uint8_t chunk[PACKAGE_HASH_CHUNK_SIZE];
    uint64_t offset = 0;
    while (offset < payload_size) {
        uint64_t remaining = payload_size - offset;
        uint32_t count = remaining < sizeof(chunk) ?
            (uint32_t)remaining : (uint32_t)sizeof(chunk);
        if (!read_at(read_context, PACKAGE_HEADER_SIZE + offset, chunk, count)) {
            return PACKAGE_IO_ERROR;
        }
        if (!sha256_update(&hash_context, chunk, count)) return PACKAGE_BAD_FORMAT;
        offset += count;
    }

    uint8_t actual_hash[32];
    if (!sha256_final(&hash_context, actual_hash)) return PACKAGE_BAD_FORMAT;
    if (!constant_time_equal(actual_hash, header + 40, sizeof(actual_hash))) {
        return PACKAGE_BAD_HASH;
    }

    struct package_info staged;
    staged.version = version;
    staged.architecture = architecture;
    staged.payload_size = payload_size;
    for (uint32_t i = 0; i < PACKAGE_KEY_ID_SIZE; i++) {
        staged.key_id[i] = header[24 + i];
    }
    for (uint32_t i = 0; i < sizeof(staged.payload_sha256); i++) {
        staged.payload_sha256[i] = actual_hash[i];
    }
    *verified_info = staged;
    return PACKAGE_OK;
}

#ifndef VOS_PACKAGE_VERIFY_H
#define VOS_PACKAGE_VERIFY_H

#include <stdint.h>

#define PACKAGE_HEADER_SIZE 136u
#define PACKAGE_MAX_PAYLOAD_BYTES (64u * 1024u * 1024u)
#define PACKAGE_KEY_ID_SIZE 16u
#define PACKAGE_SIGNATURE_SIZE 64u
#define PACKAGE_ARCH_X86_64 1u

enum package_status {
    PACKAGE_OK = 0,
    PACKAGE_INVALID = -1,
    PACKAGE_IO_ERROR = -2,
    PACKAGE_BAD_FORMAT = -3,
    PACKAGE_TOO_LARGE = -4,
    PACKAGE_NO_TRUSTED_VERIFIER = -5,
    PACKAGE_BAD_SIGNATURE = -6,
    PACKAGE_BAD_HASH = -7
};

typedef int (*package_read_at_fn)(void *context, uint64_t offset,
                                  void *buffer, uint32_t bytes);

struct package_info {
    uint16_t version;
    uint16_t architecture;
    uint64_t payload_size;
    uint8_t key_id[PACKAGE_KEY_ID_SIZE];
    uint8_t payload_sha256[32];
};

/* VOSPKG1: magic[8], version[2], architecture[2], header_size[4],
 * payload_size[8], key_id[16], SHA-256[32], Ed25519 signature[64].
 * The signature covers the domain tag "VOS-PKG-SIGN-V1\0" followed by
 * header bytes 0..71. Validates a complete file; never installs or executes.
 * Without a trusted Ed25519 verifier, all packages are rejected. */
int package_verify(package_read_at_fn read_at, void *read_context,
                   uint64_t package_size, struct package_info *verified_info);

#endif

#ifndef VOS_PACKAGE_CRYPTO_H
#define VOS_PACKAGE_CRYPTO_H

#include <stdint.h>

#include "package_verify.h"

enum package_crypto_status {
    PACKAGE_CRYPTO_NO_TRUSTED_KEY = 0,
    PACKAGE_CRYPTO_INVALID_SIGNATURE = -1,
    PACKAGE_CRYPTO_VALID_SIGNATURE = 1
};

/* Kernel-internal trust provider. It must resolve key_id only against VOS's
 * built-in public keys and apply strict Ed25519 verification. */
int package_crypto_verify_trusted(
    const uint8_t key_id[PACKAGE_KEY_ID_SIZE], const uint8_t *message,
    uint32_t message_size, const uint8_t signature[PACKAGE_SIGNATURE_SIZE]);

#endif

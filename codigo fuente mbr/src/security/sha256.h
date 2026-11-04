#ifndef VOS_SHA256_H
#define VOS_SHA256_H

#include <stdint.h>

struct sha256_context {
    uint32_t state[8];
    uint64_t total_bytes;
    uint8_t block[64];
    uint32_t block_length;
};

void sha256_init(struct sha256_context *context);
int sha256_update(struct sha256_context *context, const void *data,
                  uint64_t length);
int sha256_final(struct sha256_context *context, uint8_t digest[32]);
int sha256_digest(const void *data, uint64_t length, uint8_t digest[32]);

#endif

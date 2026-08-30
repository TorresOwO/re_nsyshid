#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} MD5_CTX;

void MD5Init(MD5_CTX *context);
void MD5Update(MD5_CTX *context, const uint8_t *input, size_t inputLen);
void MD5Final(uint8_t digest[16], MD5_CTX *context);
void md5_hash(const uint8_t *data, size_t len, uint8_t digest[16]);

#ifdef __cplusplus
}
#endif

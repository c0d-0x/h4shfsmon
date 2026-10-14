#ifndef CRYPTO_H
#define CRYPTO_H

#define HASH_FINAL 0x01
#define HASH_UPDATE 0x02

#include <openssl/core.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include "util.h"

typedef struct {
    EVP_MD_CTX *ctx;
    const EVP_MD *md;
    unsigned int size;
    unsigned char hash[];
} hash_engin_t;

hash_engin_t *new_hash_engin(void);
inline void reset_hash_engin(void *data);
bool hash_func(hash_engin_t *engin, Str_t *buf, uint8_t flag);
bool hash_file(hash_engin_t *engin, const char *path);
void print_hash(hash_engin_t *engin);

#endif  // !CRYPTO_H

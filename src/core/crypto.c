
#include "crypto.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logger.h"
#include "util.h"

hash_engin_t *new_hash_engin(void) {
    hash_engin_t *engin = malloc(sizeof(hash_engin_t));
    if (engin == NULL) OUT_OF_MEMORY();

    engin->ctx = EVP_MD_CTX_new();
    engin->md = EVP_sha256();

    assert(engin->ctx != NULL);
    assert(engin->md != NULL);
    EVP_DigestInit_ex(engin->ctx, engin->md, NULL);
    return engin;
}

inline void reset_hash_engin(void *data) {
    hash_engin_t *engin = (hash_engin_t *) data;
    EVP_MD_CTX_reset(engin->ctx);
    EVP_DigestInit_ex(engin->ctx, engin->md, NULL);
}

inline void cleanup_hash_engin(hash_engin_t *engin) {
    EVP_MD_CTX_free(engin->ctx);
    EVP_MD_free((EVP_MD *) engin->md);
    free(engin);
}

bool hash_func(hash_engin_t *engin, Str_t *buf, uint8_t flag) {
    if (flag & HASH_UPDATE) return EVP_DigestUpdate(engin->ctx, (void *) buf->str, buf->len) ? true : false;
    if (flag & HASH_FINAL) return EVP_DigestFinal_ex(engin->ctx, engin->hash, &engin->size) ? true : false;
    return false;
}

void print_hash(hash_engin_t *engin) {
    for (uint32_t i = 0; i < engin->size; i++) fprintf(stdout, "%02x", engin->hash[i]);
    fprintf(stdout, "\n");
}

bool hash_file(hash_engin_t *engin, const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        log_error("Failed to open file: [ %s ] %s\n", path, strerror(errno));
        return false;
    }

    Str_t *buf = malloc(sizeof(Str_t) + sizeof(char) * 1024);
    if (buf == NULL) OUT_OF_MEMORY();

    bool ret = false;
    while (true) {
        if ((buf->len = fread(buf->str, sizeof(char), 1024, fp)) <= 0) {
            ret = hash_func(engin, NULL, HASH_FINAL);
            break;
        }

        if (!(ret = hash_func(engin, buf, HASH_UPDATE))) {
            log_error("Failed to hash buff");
            break;
        }
    }

    fclose(fp);
    return ret;
}

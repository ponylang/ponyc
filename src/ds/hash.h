#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stddef.h>

uint64_t siphash24(const char* key, const char *in, size_t len);

uint64_t hash(void* in, size_t len);

uint64_t strhash(const char* str);

uint64_t ptrhash(const void* p);

uint64_t inthash(uint64_t key);

#endif

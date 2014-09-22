#ifndef HASH_H
#define HASH_H

#include <platform.h>
#include <stdint.h>
#include <stddef.h>

PONY_EXTERN_C_BEGIN

uint64_t siphash24(const unsigned char* key, const char *in, size_t len);

uint64_t hash(void* in, size_t len);

uint64_t strhash(const char* str);

uint64_t ptrhash(const void* p);

uint64_t inthash(uint64_t key);

size_t next_pow2(size_t v);

PONY_EXTERN_C_END

#endif

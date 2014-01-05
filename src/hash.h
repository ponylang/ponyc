#ifndef HASH_H
#define HASH_H

#include <stdint.h>

uint64_t strhash( const void* str, uint64_t seed );
uint64_t ptrhash( const void* p, uint64_t seed );

#endif

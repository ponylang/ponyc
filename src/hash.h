#ifndef HASH_H
#define HASH_H

#include <stdint.h>

uint64_t strhash( const char* str );
uint64_t ptrhash( const void* p );

#endif

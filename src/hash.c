/*
Adapted from http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
*/

#include "hash.h"
#include <string.h>

static inline uint64_t rotl64( uint64_t x, int8_t r )
{
  return (x << r) | (x >> (64 - r));
}

static inline uint64_t fmix( uint64_t k )
{
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdLLU;
  k ^= k >> 33;
  k *= 0xc4ceb9fe1a85ec53LLU;
  k ^= k >> 33;

  return k;
}

void MurmurHash3_x64_128( const void* key, const int len, const uint32_t seed, void* out )
{
  const uint8_t* data = (const uint8_t*)key;
  const int nblocks = len / 16;

  uint64_t h1 = seed;
  uint64_t h2 = seed;

  const uint64_t c1 = 0x87c37b91114253d5LLU;
  const uint64_t c2 = 0x4cf5ad432745937fLLU;
  const uint64_t* blocks = (const uint64_t*)data;

  uint64_t k1, k2;

  for( int i = 0; i < nblocks; i++ )
  {
    k1 = blocks[(i * 2) + 0];
    k2 = blocks[(i * 2) + 1];

    k1 *= c1;
    k1 = rotl64( k1, 31 );
    k1 *= c2;
    h1 ^= k1;

    h1 = rotl64( h1, 27 );
    h1 += h2;
    h1 = (h1 * 5) + 0x52dce729;

    k2 *= c2;
    k2 = rotl64( k2, 33 );
    k2 *= c1;
    h2 ^= k2;

    h2 = rotl64( h2, 31 );
    h2 += h1;
    h2 = (h2 * 5) + 0x38495ab5;
  }

  const uint8_t* tail = (const uint8_t*)(data + (nblocks * 16));

  k1 = 0;
  k2 = 0;

  switch( len & 15 )
  {
  case 15: k2 ^= ((uint64_t)tail[14]) << 48;
  case 14: k2 ^= ((uint64_t)tail[13]) << 40;
  case 13: k2 ^= ((uint64_t)tail[12]) << 32;
  case 12: k2 ^= ((uint64_t)tail[11]) << 24;
  case 11: k2 ^= ((uint64_t)tail[10]) << 16;
  case 10: k2 ^= ((uint64_t)tail[ 9]) << 8;

  case 9:
    k2 ^= ((uint64_t)tail[ 8]) << 0;
    k2 *= c2;
    k2 = rotl64( k2, 33 );
    k2 *= c1;
    h2 ^= k2;

  case 8: k1 ^= ((uint64_t)tail[ 7]) << 56;
  case 7: k1 ^= ((uint64_t)tail[ 6]) << 48;
  case 6: k1 ^= ((uint64_t)tail[ 5]) << 40;
  case 5: k1 ^= ((uint64_t)tail[ 4]) << 32;
  case 4: k1 ^= ((uint64_t)tail[ 3]) << 24;
  case 3: k1 ^= ((uint64_t)tail[ 2]) << 16;
  case 2: k1 ^= ((uint64_t)tail[ 1]) << 8;

  case 1:
    k1 ^= ((uint64_t)tail[ 0]) << 0;
    k1 *= c1;
    k1 = rotl64( k1, 31 );
    k1 *= c2;
    h1 ^= k1;
  };

  h1 ^= len;
  h2 ^= len;

  h1 += h2;
  h2 += h1;

  h1 = fmix( h1 );
  h2 = fmix( h2 );

  h1 += h2;
  h2 += h1;

  ((uint64_t*)out)[0] = h1;
  ((uint64_t*)out)[1] = h2;
}

uint64_t strhash( const char* str )
{
  uint64_t out[2];
  size_t len = strlen( str );
  MurmurHash3_x64_128( str, len, 0, out );
  return out[0] ^ out[1];
}

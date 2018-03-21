#include "fun.h"
#include <stdio.h>
#include <string.h>

#include <platform.h>


#ifdef PLATFORM_IS_ILP32
/*
   SipHash reference C implementation

   Copyright (c) 2016 Jean-Philippe Aumasson <jeanphilippe.aumasson@gmail.com>

   To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   You should have received a copy of the CC0 Public Domain Dedication along
   with
   this software. If not, see
   <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

/* default: SipHash-2-4 */
#define cROUNDS 2
#define dROUNDS 4

static const unsigned char the_key[16] = {
  0xFE, 0x09, 0xD3, 0x22, 0x6B, 0x9C, 0x10, 0x8A,
  0xE1, 0x35, 0x72, 0xB5, 0xCC, 0x3F, 0x92, 0x9F
};

#define ROTL(x, b) (uint32_t)(((x) << (b)) | ((x) >> (32 - (b))))

#define SIPROUND                                                               \
    do {                                                                       \
        v0 += v1;                                                              \
        v1 = ROTL(v1, 5);                                                      \
        v1 ^= v0;                                                              \
        v0 = ROTL(v0, 16);                                                     \
        v2 += v3;                                                              \
        v3 = ROTL(v3, 8);                                                      \
        v3 ^= v2;                                                              \
        v0 += v3;                                                              \
        v3 = ROTL(v3, 7);                                                      \
        v3 ^= v0;                                                              \
        v2 += v1;                                                              \
        v1 = ROTL(v1, 13);                                                     \
        v1 ^= v2;                                                              \
        v2 = ROTL(v2, 16);                                                     \
    } while (0)


size_t halfsiphash(const uint8_t *k, const char *in, const size_t inlen)
  {
    uint32_t v0 = 0;
    uint32_t v1 = 0;
    uint32_t v2 = 0x6c796765;
    uint32_t v3 = 0x74656462;
    uint32_t k0 = *(uint32_t*)(k);
    uint32_t k1 = *(uint32_t*)(k + 4);
    uint32_t m;
    int i;
    const char* end = in + inlen - (inlen % sizeof(uint32_t));
    const int left = inlen & 3;
    uint32_t b = ((uint32_t)inlen) << 24;
    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;

    for (; in != end; in += 4)
      {
	m = *(uint32_t*)(in);
        v3 ^= m;

        for (i = 0; i < cROUNDS; ++i)
            SIPROUND;

        v0 ^= m;
      }

    switch (left) {
    case 3:
        b |= ((uint32_t)in[2]) << 16;
    case 2:
        b |= ((uint32_t)in[1]) << 8;
    case 1:
        b |= ((uint32_t)in[0]);
        break;
    case 0:
        break;
    }

    v3 ^= b;

    for (i = 0; i < cROUNDS; ++i)
        SIPROUND;

    v0 ^= b;

    v2 ^= 0xff;

    for (i = 0; i < dROUNDS; ++i)
        SIPROUND;

    return v0 ^ v1 ^ v2 ^ v3;
}
#else
static const unsigned char the_key[16] = {
  0xFE, 0x09, 0xD3, 0x22, 0x6B, 0x9C, 0x10, 0x8A,
  0xE1, 0x35, 0x72, 0xB5, 0xCC, 0x3F, 0x92, 0x9F
};

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define SIPROUND \
  do { \
    v0 += v1; v1=ROTL(v1,13); v1 ^= v0; v0=ROTL(v0,32); \
    v2 += v3; v3=ROTL(v3,16); v3 ^= v2; \
    v0 += v3; v3=ROTL(v3,21); v3 ^= v0; \
    v2 += v1; v1=ROTL(v1,17); v1 ^= v2; v2=ROTL(v2,32); \
  } while(0)


static uint64_t siphash24(const unsigned char* key, const char* in, size_t len)
{
  uint64_t k0 = *(uint64_t*)(key);
  uint64_t k1 = *(uint64_t*)(key + 8);
  uint64_t b = (uint64_t)len << 56;

  uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
  uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
  uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
  uint64_t v3 = k1 ^ 0x7465646279746573ULL;

  const char* end = (in + len) - (len % 8);

  for(; in != end; in += 8)
  {
    uint64_t m = *(uint64_t*)in;
    v3 ^= m;
    SIPROUND;
    SIPROUND;
    v0 ^= m;
  }

  switch(len & 7)
  {
    case 7: b |= ((uint64_t)in[6]) << 48; // fallthrough
    case 6: b |= ((uint64_t)in[5]) << 40; // fallthrough
    case 5: b |= ((uint64_t)in[4]) << 32; // fallthrough
    case 4: b |= ((uint64_t)in[3]) << 24; // fallthrough
    case 3: b |= ((uint64_t)in[2]) << 16; // fallthrough
    case 2: b |= ((uint64_t)in[1]) << 8;  // fallthrough
    case 1: b |= ((uint64_t)in[0]);
  }

  v3 ^= b;
  SIPROUND;
  SIPROUND;
  v0 ^= b;
  v2 ^= 0xff;
  SIPROUND;
  SIPROUND;
  SIPROUND;
  SIPROUND;

  return v0 ^ v1 ^ v2 ^ v3;
}
#endif

PONY_API size_t ponyint_hash_block(const void* p, size_t len)
{
#ifdef PLATFORM_IS_ILP32
  return halfsiphash(the_key, (const char*) p, len);
#else
  return siphash24(the_key, (const char*) p, len);
#endif
}


size_t ponyint_hash_str(const char* str)
{
#ifdef PLATFORM_IS_ILP32
  return halfsiphash(the_key, str, strlen(str));
#else
  return siphash24(the_key, str, strlen(str));
#endif
}

size_t ponyint_hash_ptr(const void* p)
{
#ifdef PLATFORM_IS_ILP32
  return ponyint_hash_int32((uintptr_t)p);
#else
  return ponyint_hash_int64((uintptr_t)p);
#endif
}

uint64_t ponyint_hash_int64(uint64_t key)
{
  key = ~key + (key << 21);
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8);
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4);
  key = key ^ (key >> 28);
  key = key + (key << 31);

  return key;
}

uint32_t ponyint_hash_int32(uint32_t key)
{
  key = ~key + (key << 15);
  key = key ^ (key >> 12);
  key = key + (key << 2);
  key = key ^ (key >> 4);
  key = (key + (key << 3)) + (key << 11);
  key = key ^ (key >> 16);
  return key;
}

size_t ponyint_hash_size(size_t key)
{
#ifdef PLATFORM_IS_ILP32
  return ponyint_hash_int32(key);
#else
  return ponyint_hash_int64(key);
#endif
}

size_t ponyint_next_pow2(size_t i)
{
#ifdef PLATFORM_IS_ILP32
  i--;
// On ARM 5 and higher, the clz instruction gives the correct result for 0.
#  ifndef ARMV5
  if(i == 0)
    i = 32;
  else
#  endif
    i = __pony_clzl(i);
  return 1 << (i == 0 ? 0 : 32 - i);
#else
  i--;
#  ifndef ARMV5
  if(i == 0)
    i = 64;
  else
#  endif
    i = __pony_clzl(i);
  // Cast is needed for optimal code generation (avoids an extension).
  return (size_t)1 << (i == 0 ? 0 : 64 - i);
#endif
}

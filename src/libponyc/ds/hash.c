#include "hash.h"
#include <string.h>

#include <platform.h>

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

uint64_t siphash24(const unsigned char* key, const char *in, size_t len)
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
    case 7: b |= ((uint64_t)in[6]) << 48;
    case 6: b |= ((uint64_t)in[5]) << 40;
    case 5: b |= ((uint64_t)in[4]) << 32;
    case 4: b |= ((uint64_t)in[3]) << 24;
    case 3: b |= ((uint64_t)in[2]) << 16;
    case 2: b |= ((uint64_t)in[1]) << 8;
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

uint64_t hash(void* in, size_t len)
{
  return siphash24(the_key, (const char*)in, len);
}

uint64_t strhash(const char* str)
{
  return siphash24(the_key, str, strlen(str));
}

uint64_t ptrhash(const void* p)
{
  return inthash((uint64_t)p);
}

uint64_t inthash(uint64_t key)
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

size_t next_pow2(size_t v)
{
  v--;

  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;

  return v + 1;
}

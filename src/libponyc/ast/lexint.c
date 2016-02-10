#include "lexint.h"
#include <stdio.h>
#include <math.h>

#if !defined(PLATFORM_IS_ILP32) && !defined(PLATFORM_IS_WINDOWS)
#define USE_NATIVE128
#define NATIVE(a, b) \
  __uint128_t a = ((__uint128_t)(b)->high << 64) | (b)->low;
#define LEXINT(a, b) \
  (b)->low = (uint64_t)(a); \
  (b)->high = (uint64_t)((a) >> 64);
#endif

void lexint_zero(lexint_t* i)
{
  i->low = 0;
  i->high = 0;
}

int lexint_cmp(lexint_t* a, lexint_t* b)
{
  if(a->high > b->high)
    return 1;

  if(a->high < b->high)
    return -1;

  if(a->low > b->low)
    return 1;

  if(a->low < b->low)
    return -1;

  return 0;
}

int lexint_cmp64(lexint_t* a, uint64_t b)
{
  if(a->high > 0)
    return 1;

  if(a->low > b)
    return 1;

  if(a->low < b)
    return -1;

  return 0;
}

void lexint_shl(lexint_t* dst, lexint_t* a, uint64_t b)
{
  if(b >= 128)
  {
    lexint_zero(dst);
  } else if(b > 64) {
    dst->high = a->low << (b - 64);
    dst->low = 0;
  } else if(b == 64) {
    dst->high = a->low;
    dst->low = 0;
  } else if(b > 0) {
    dst->high = (a->high << b) + (a->low >> (64 - b));
    dst->low = a->low << b;
  } else {
    dst->high = a->high;
    dst->low = a->low;
  }
}

void lexint_shr(lexint_t* dst, lexint_t* a, uint64_t b)
{
  if(b >= 128)
  {
    lexint_zero(dst);
  } else if(b > 64) {
    dst->low = a->high >> (b - 64);
    dst->high = 0;
  } else if(b == 64) {
    dst->low = a->high;
    dst->high = 0;
  } else if(b > 0) {
    dst->low = (a->high << (64 - b)) + (a->low >> b);
    dst->high = a->high >> b;
  } else {
   dst->high = a->high;
   dst->low = a->low;
  }
}

uint64_t lexint_testbit(lexint_t* a, uint8_t b)
{
  if(b >= 64)
    return (a->high >> (b - 64)) & 1;

  return (a->low >> b) & 1;
}

void lexint_setbit(lexint_t* dst, lexint_t* a, uint8_t b)
{
  *dst = *a;

  if(b >= 64)
    dst->high |= (uint64_t)1 << (b - 64);
  else
    dst->low |= (uint64_t)1 << b;
}

void lexint_add(lexint_t* dst, lexint_t* a, lexint_t* b)
{
  dst->high = a->high + b->high + ((a->low + b->low) < a->low);
  dst->low = a->low + b->low;
}

void lexint_add64(lexint_t* dst, lexint_t* a, uint64_t b)
{
  dst->high = a->high + ((a->low + b) < a->low);
  dst->low = a->low + b;
}

void lexint_sub(lexint_t* dst, lexint_t* a, lexint_t* b)
{
  dst->high = a->high - b->high - ((a->low - b->low) > a->low);
  dst->low = a->low - b->low;
}

void lexint_sub64(lexint_t* dst, lexint_t* a, uint64_t b)
{
  dst->high = a->high - ((a->low - b) > a->low);
  dst->low = a->low - b;
}

void lexint_mul64(lexint_t* dst, lexint_t* a, uint64_t b)
{
#ifdef USE_NATIVE128
  NATIVE(v1, a);
  __uint128_t v2 = v1 * b;
  LEXINT(v2, dst);
#else
  lexint_t t = *a;
  lexint_zero(dst);

  while(b > 0)
  {
    if((b & 1) != 0)
      lexint_add(dst, dst, &t);

    lexint_shl(&t, &t, 1);
    b >>= 1;
  }
#endif
}

void lexint_div64(lexint_t* dst, lexint_t* a, uint64_t b)
{
#ifdef USE_NATIVE128
  NATIVE(v1, a);
  __uint128_t v2 = v1 / b;
  LEXINT(v2, dst);
#else
  lexint_t o = *a;
  lexint_zero(dst);

  if(b == 0)
    return;

  if(b == 1)
  {
    *dst = o;
    return;
  }

  lexint_t r, t;
  lexint_zero(&r);

  for(uint8_t i = 127; i < UINT8_MAX; i--)
  {
    lexint_shl(&r, &r, 1);
    lexint_shr(&t, &o, i);
    r.low |= t.low & 1;

    if(lexint_cmp64(&r, b) >= 0)
    {
      lexint_sub64(&r, &r, b);
      lexint_setbit(dst, dst, i);
    }
  }
#endif
}

void lexint_char(lexint_t* i, int c)
{
  i->high = (i->high << 8) | (i->low >> 56);
  i->low = (i->low << 8) | c;
}

bool lexint_accum(lexint_t* i, uint64_t digit, uint64_t base)
{
#ifdef USE_NATIVE128
  NATIVE(v1, i);
  __uint128_t v2 = v1 * base;

  if((v2 / base) != v1)
    return false;

  v2 += digit;

  if(v2 < v1)
    return false;

  LEXINT(v2, i);
#else
  lexint_t v2;
  lexint_mul64(&v2, i, base);

  lexint_t v3;
  lexint_div64(&v3, &v2, base);

  if(lexint_cmp(&v3, i) != 0)
  {
    lexint_div64(&v3, &v2, base);
    return false;
  }

  lexint_add64(&v2, &v2, digit);

  if(lexint_cmp(&v2, i) < 0)
    return false;

  *i = v2;
#endif
  return true;
}

// Support for clz (count leading zeros) is suprisingly platform, and even CPU,
// dependent. Therefore, since we're not overly concerned with performance
// within the lexer, we provide a software version here.
static int count_leading_zeros(uint64_t n)
{
  if(n == 0)
    return 64;

  int count = 0;

  if((n >> 32) == 0) { count += 32; n <<= 32; }
  if((n >> 48) == 0) { count += 16; n <<= 16; }
  if((n >> 56) == 0) { count += 8; n <<= 8; }
  if((n >> 60) == 0) { count += 4; n <<= 4; }
  if((n >> 62) == 0) { count += 2; n <<= 2; }
  if((n >> 63) == 0) { count += 1; n <<= 1; }

  return count;
}

double lexint_double(lexint_t* i)
{
  if(i->low == 0 && i->high == 0)
    return 0;

  int sig_bit_count = 128 - count_leading_zeros(i->high);

  if(i->high == 0)
    sig_bit_count = 64 - count_leading_zeros(i->low);

  uint64_t exponent = sig_bit_count - 1;
  uint64_t mantissa = i->low;

  if(sig_bit_count <= 53)
  {
    // We can represent this number exactly.
    mantissa <<= (53 - sig_bit_count);
  }
  else
  {
    // We can't exactly represents numbers of this size, have to truncate bits.
    // We have to round, first shift so we have 55 bits of mantissa.
    if(sig_bit_count == 54)
    {
      mantissa <<= 1;
    }
    else if(sig_bit_count > 55)
    {
      lexint_t t;
      lexint_shr(&t, i, sig_bit_count - 55);
      mantissa = t.low;
      lexint_shl(&t, &t, sig_bit_count - 55);

      if(lexint_cmp(&t, i) != 0)
      {
        // Some of the bits we're discarding are non-0. Round up mantissa.
        mantissa |= 1;
      }
    }

    // Round first 53 bits of mantissa to even an ditch extra 2 bits.
    if((mantissa & 4) != 0)
      mantissa |= 1;

    mantissa = (mantissa + 1) >> 2;

    if((mantissa & (1ULL << 53)) != 0)
    {
      mantissa >>= 1;
      exponent += 1;
    }
  }

  uint64_t raw_bits = ((exponent + 1023) << 52) | (mantissa & 0xFFFFFFFFFFFFF);
  double* fp_bits = (double*)&raw_bits;
  return *fp_bits;
}

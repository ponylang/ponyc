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
  } else {
    dst->high = (a->high << b) + (a->low >> (64 - b));
    dst->low = a->low << b;
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
  } else {
    dst->low = (a->high << (64 - b)) + (a->low >> b);
    dst->high = a->high >> b;
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
#if !defined(PLATFORM_IS_ILP32) && !defined(PLATFORM_IS_WINDOWS)
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

double lexint_double(lexint_t* i)
{
  if((int64_t)i->high < 0)
  {
    // Treat as a signed number.
    lexint_t t = *i;
    t.high = ~t.high;
    t.low = ~t.low;
    lexint_add64(&t, &t, 1);

    return -(((double)t.high * ldexp(1.0, 64)) + (double)t.low);
  }

  return ((double)i->high * ldexp(1.0, 64)) + (double)i->low;
}

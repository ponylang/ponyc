#ifndef BIGINT_BIGINT_H
#define BIGINT_BIGINT_H

/** 128-bit integer support for (little endian) 64-bit platforms.
 *
 */
#include "unsigned.h"
#include "signed.h"

typedef UnsignedInt128 __uint128_t;
typedef SignedInt128 __int128_t;

inline __uint128_t pow(double e, __int128_t b)
{
  //only need integral pow.
  int integral_e = (int)e;

  //we only target unsigned results for now.
  __uint128_t base = b.magnitude;
  __uint128_t res = 1;

  while (integral_e != 0)
  {
    if (integral_e & 1)
    {
      //multiply, as integral_e is not a power of 2
      res = res * base;
    }

    if ((integral_e >>= 1) != 0)
    {
      //square
      base *= base;
    }
  }

  //only support unsigned result.
	return res;
}

#endif
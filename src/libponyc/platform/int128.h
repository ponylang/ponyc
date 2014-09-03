#ifndef PLATFORM_INT128_H
#define PLATFORM_INT128_H

#include <stdint.h>

/** 128-bit integer support for (little endian) 64-bit platforms.
 *
 */
#include "unsigned.h"
#include "signed.h"

typedef UnsignedInt128 __uint128_t;
typedef SignedInt128 __int128_t;

inline double pow(double b, SignedInt128& e)
{
  bool div = (e.sign == negative);
  UnsignedInt128 res = 1;

  switch(e.sign)
  {
    case zero:
      return (double)uint128_1.low;
    case negative: //b^(-e) == 1/(b^e)
    case positive: //fallthrough, computes b^e
    {
      while(e.magnitude != uint128_0)
      {
        if((e.magnitude & uint128_1) != uint128_0)
          res = res * b;

        if((e.magnitude >>= 1) != uint128_0)
          b *= b;
      }
    }
  }

  return (div ? 1/(double)res.low : (double)res.low);
}

#endif

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
	return 0;
}

#endif
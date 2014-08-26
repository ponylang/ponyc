#ifndef BIGINT_BIGINT_H
#define BIGINT_BIGINT_H

#include <stddef.h>

/** BigInteger support for (little endian) 64-bit platforms.
 *
 */
template<size_t words> class BigUnsignedInt;
template<size_t words> class BigSigndInt;

#include "base.h"
#include "unsigned.h"
#include "signed.h"

typedef BigUnsignedInt<2> __uint128_t;
typedef BigSignedInt<2> __int128_t;

inline __uint128_t pow(double e, __int128_t b)
{
	return 0;
}

#endif
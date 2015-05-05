#include <math.h>
#include <inttypes.h>

/*
 * For 32-bit systems, glibc does not provide __int128 conversion
 * functions probably because gcc doesn't have an __int128 type for
 * such targets.
 *
 * Here are some pretty horrible conversions.
 */

typedef struct { int64_t hi; uint64_t lo; } __int128_t;		/* TItype */
typedef struct { uint64_t hi; uint64_t lo; } __uint128_t;	/* UTItype */
typedef float DFtype __attribute__((mode (DF)));
typedef float SFtype __attribute__((mode (SF)));

#define weak(f) __attribute__((weak, alias (#f)))

__int128_t  __fixdfti    (DFtype      f) weak(horrid_fixdfti);
__int128_t  __fixsfti    (SFtype      f) weak(horrid_fixsfti);
__uint128_t __fixunsdfti (DFtype      f) weak(horrid_fixunsdfti);
__uint128_t __fixunssfti (SFtype      f) weak(horrid_fixunsdfti);
DFtype      __floattidf  (__int128_t  i) weak(horrid_floattidf);
SFtype      __floattisf  (__int128_t  i) weak(horrid_floattisf);
DFtype      __floatuntidf(__uint128_t i) weak(horrid_floatuntidf);
SFtype      __floatuntisf(__uint128_t i) weak(horrid_floatuntisf);

__int128_t
horrid_fixdfti(DFtype f)
{
	__int128_t r;
	r.hi = (int64_t)(f / 1.8446744073709551616e19);
	r.lo = (uint64_t)(int64_t)remainder(f, 1.8446744073709551616e19);
	return r;
}

__int128_t
horrid_fixsfti(SFtype f)
{
	__int128_t r;
	r.hi = (int64_t)(f / 1.8446744073709551616e19);
	r.lo = (uint64_t)(int64_t)remainder(f, 1.8446744073709551616e19);
	return r;
}

__uint128_t
horrid_fixunsdfti(DFtype f)
{
	__uint128_t r;
	r.hi = (int64_t)(f / 1.8446744073709551616e19);
	r.lo = (uint64_t)(int64_t)remainder(f, 1.8446744073709551616e19);
	return r;
}

__uint128_t
horrid_fixunssfti(SFtype f)
{
	__uint128_t r;
	r.hi = (int64_t)(f / 1.8446744073709551616e19);
	r.lo = (uint64_t)(int64_t)remainder(f, 1.8446744073709551616e19);
	return r;
}

DFtype
horrid_floattidf(__int128_t i)
{
	return (DFtype)((double)i.lo + 1.8446744073709551616e19 * (double)i.hi);
}

SFtype
horrid_floattisf(__int128_t i)
{
	return (SFtype)((double)i.lo + 1.8446744073709551616e19 * (double)i.hi);
}

DFtype
horrid_floatuntidf(__uint128_t i)
{
	return (DFtype)((double)i.lo + 1.8446744073709551616e19 * (double)i.hi);
}

SFtype
horrid_floatuntisf(__uint128_t i)
{
	return (SFtype)((double)i.lo + 1.8446744073709551616e19 * (double)i.hi);
}


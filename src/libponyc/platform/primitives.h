#ifndef PLATFORM_PRIMITIVES_H
#define PLATFORM_PRIMITIVES_H

/** Macros to simulate primitive operations on __uint128_t.
 *  
 *  The obvious (and cleaner) approach would have been to implement a class
 *  "__uint128_t" and overload operators such as ++ and so on. However, we use
 *  128-bit integers in structs and unions. MSVC++ does not supported 
 *  unrestricted structs and unions. Hence, we need to go for this macro 
 *  non-sense, unless we can agree to remove uin128_ts from structs and unions.
 */

#ifdef PLATFORM_IS_WINDOWS

#endif

#endif
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/** Convenience macro for making extern "C" more succinct.
 *
 */
#if defined(__cplusplus) && !defined(_MSC_VER)
#  define PONY_EXTERN_C_BEGIN extern "C" {
#  define PONY_EXTERN_C_END }
#else
#  define PONY_EXTERN_C_BEGIN
#  define PONY_EXTERN_C_END
#endif

/** Determines the operating system.
 *
 */
#if defined(__APPLE__) && defined(__MACH__)
#  define PLATFORM_IS_MACOSX
#elif defined(__linux__)
#  define PLATFORM_IS_LINUX
#elif defined(_WIN64)
#  define PLATFORM_IS_WINDOWS
#  if defined(_MSC_VER)
#    define PLATFORM_IS_VISUAL_STUDIO
/** Disable warnings about default constructors and class operators.
 *
 *  Visual studio complains about the lack of default constructors for
 *  standard C structs (4510). The same applies for assignment operators
 *  (4512). We also do not want to be warned about structs that cannot be
 *  instantiated due to missing default constructors (4610).
 *
 *  http://msdn.microsoft.com/en-us/library/2cf74y2b.aspx
 *  http://msdn.microsoft.com/en-us/library/hsyx7kbz.aspx
 *  http://msdn.microsoft.com/en-us/library/92d6x4xw.aspx
 */
#  pragma warning(disable:4510)
#  pragma warning(disable:4512)
#  pragma warning(disable:4610)
/** Disable warning about __declspec(restrict) not being used at function
 *  definition. According to the documentation, it should not matter.
 *  Also, using extern "C" does not remove this warning.
 *
 *  http://msdn.microsoft.com/en-us/library/8bcxafdh.aspx
 *  http://msdn.microsoft.com/en-us/library/1aysk3y8.aspx
 */
#  pragma warning(disable:4565)
/** Allow formal parameters of functions to remain unused (e.g. actor disp.)
 *
 *  http://msdn.microsoft.com/en-us/library/26kb9fy0.aspx
 */
#  pragma warning(disable:4100)
/** Allow constant conditional expressions (e.g. while(true)).
 *
 *  Microsoft advises to replace expressions like while(true) with for(;;).
 *  http://msdn.microsoft.com/en-us/library/6t66728h%28v=vs.90%29.aspx
 */
#  pragma warning(disable:4127)
/** Although not a standard extension, we want to allow nameless structs and
 *  unions.
 *
 *  http://msdn.microsoft.com/en-us/library/c89bw853.aspx
 */
#  pragma warning(disable:4201)
/** If we pad a structure using __pony_spec_align__ then we really thought
 *  about this before!
 *
 *  http://msdn.microsoft.com/en-us/library/92fdk6xx.aspx
 */
#  pragma warning(disable:4324)
/** Assignments within conditionals expressions are fine, too.
 *
 *  http://msdn.microsoft.com/en-us/library/7hw7c1he.aspx
 */
#  pragma warning(disable:4706)
#endif
#  include <Windows.h>
#else
#  error PLATFORM NOT SUPPORTED!
#endif

#if defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_LINUX)
# define PLATFORM_IS_POSIX_BASED
#endif

#if defined(PLATFORM_IS_POSIX_BASED) || defined(__MINGW64__)
#  define PLATFORM_IS_CLANG_OR_GCC
#endif

/** Data types.
 *
 */
#ifdef PLATFORM_IS_VISUAL_STUDIO
  #include <BaseTsd.h> //Microsoft's version of stddef.h
  typedef SSIZE_T ssize_t;
  typedef SIZE_T size_t;
#elif defined(PLATFORM_IS_CLANG_OR_GCC)
  #include <stddef.h>
#endif

#define PONY_ERRNO uint32_t

/** Format specifiers and snprintf.
 *
 */
#ifdef PLATFORM_IS_VISUAL_STUDIO
#  include <stdarg.h>
#  include <stdio.h>
// Make __attribute__ annotations (e.g. for checking
// printf-like functions a no-op for Visual Studio.
// That way, the known semantics of __attribute__(...)
// remains clear and no wrapper needs to be used.
#  define __attribute__(X)
#  define __zu "%Iu"
#  define strdup _strdup

inline int snprintf(char* str, size_t size, const char* format, ...)
{
  int written;
  va_list argv;

  va_start(argv, format);
  written = vsnprintf(str, size, format, argv);
  va_end(argv);

  return written;
}
#else
#  define __zu "%zu"
#endif

/** Standard builtins.
 *
 */
#ifdef PLATFORM_IS_CLANG_OR_GCC
#  define __pony_popcount(X) __builtin_popcount( (X) )
#  define __pony_popcount64(X) __pony_popcount(X)
#  define __pony_ffs(X) __builtin_ffs( (X) )
#  define __pony_ffsl(X) __builtin_ffsl( (X) )
#  define __pony_clz(X) __builtin_clz( (X) )
#  define __pony_clzl(X) __builtin_clzl( (X) )
#  ifdef __clang__
#    define __pony_rdtsc() __builtin_readcyclecounter()
#  else
#    define __pony_rdtsc() __builtin_ia32_rdtsc()
#  endif
#else
#  include <intrin.h>
#  define __pony_popcount(X) __popcnt(( X ))
#  define __pony_popcount64(X) __popcnt64(( X ))

static __declspec(thread) DWORD lsb;

#  define __pony_ffs(X) (lsb = 0, _BitScanForward(&lsb, (X)), lsb+1)
#  define __pony_ffsl(X) (lsb = 0, _BitScanForward64(&lsb, (X)), lsb+1)
#  define __pony_clz(X) (lsb = 0,_BitScanReverse(&lsb, (X)), lsb)
#  define __pony_clzl(X) (lsb = 0, _BitScanReverse64(&lsb, (X)), lsb)
#  define __pony_rdtsc() __rdtsc()
#endif

#ifdef PLATFORM_IS_VISUAL_STUDIO
#  include <malloc.h>
#  define VLA(TYPE, NAME, SIZE) TYPE* NAME = (TYPE*) alloca(\
            (SIZE)*sizeof(TYPE))
#endif
#if defined(PLATFORM_IS_POSIX_BASED) || defined(PLATFORM_IS_CLANG_OR_GCC)
#  define VLA(TYPE, NAME, SIZE) TYPE NAME[(SIZE)]
#endif

/** Storage class modifiers.
 *
 */
#ifdef PLATFORM_IS_VISUAL_STUDIO
#  define __pony_spec_align__(IDENT, BYTES) \
              __declspec(align(BYTES)) IDENT
#elif defined(PLATFORM_IS_CLANG_OR_GCC)
#  define __pony_spec_align__(IDENT, BYTES) \
              IDENT __attribute__( (aligned (BYTES)) )
#endif

#ifdef PLATFORM_IS_VISUAL_STUDIO
#  define __pony_spec_malloc__(FUNC, ...) \
            __declspec(restrict) FUNC
#elif defined(PLATFORM_IS_CLANG_OR_GCC)
#  define __pony_spec_malloc__(FUNC, ...) \
            FUNC __attribute__((malloc))
#endif

/** Compile time choose expression.
 *
 *  (void)0 will cause a compile-time error in non-cpp environments, as
 *  __pony_choose_expr is based on __builtin_choose_expr.
 */
#if defined(PLATFORM_IS_VISUAL_STUDIO) || defined(__cplusplus)
#  define EXPR_NONE 0
#  define __pony_choose_expr(COND, THEN, ELSE) \
            (( COND ) ? ( THEN ) : ( ELSE ))
#elif defined(PLATFORM_IS_CLANG_OR_GCC)
#  define EXPR_NONE ((void) 0)
#  define __pony_choose_expr(COND, THEN, ELSE) \
            __builtin_choose_expr(COND, THEN, ELSE)
#endif

#include "atomics.h"
#include "threads.h"
#include "alloc.h"
#include "paths.h"

#if defined(PLATFORM_IS_WINDOWS)
#  include "vcvars.h"
#endif

#if defined(PLATFORM_IS_VISUAL_STUDIO)
#  if defined(PONY_USE_BIGINT)
#    include "int128.h"
#  else
     typedef struct __int128_t { uint64_t low; int64_t high; } __int128_t;
     typedef struct __uint128_t { uint64_t low; uint64_t high; } __uint128_t;
#  endif
#endif

#endif

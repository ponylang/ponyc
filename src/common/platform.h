#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/** Convenience macro for making extern "C" more succinct.
 *
 */
#if defined(__cplusplus)
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
#elif defined(__FreeBSD__)
#  define PLATFORM_IS_FREEBSD
#elif defined(_WIN32)
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
#  pragma warning(disable:4146)
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
/** VS2015 warns about a missing delete in Clang 3.6.  Delete actually exists.
 *
 * https://msdn.microsoft.com/en-us/library/cxdxz3x6.aspx
 */
#  pragma warning(disable:4291)
#endif
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <Windows.h>
#else
#  error PLATFORM NOT SUPPORTED!
#endif

#if defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_LINUX) || defined (PLATFORM_IS_FREEBSD)
#  define PLATFORM_IS_POSIX_BASED
#endif

#if defined(PLATFORM_IS_POSIX_BASED) || defined(__MINGW64__)
#  define PLATFORM_IS_CLANG_OR_GCC
#endif

/** The platform's programming model.
 *
 */
#if defined(__LP64__)
#  define PLATFORM_IS_LP64
#elif defined(_WIN64)
#  define PLATFORM_IS_LLP64
#else
#  define PLATFORM_IS_ILP32
#endif

/** ARM architecture flags.
 *
 */
#if defined(__ARM_ARCH_7__) || \
    defined(__ARM_ARCH_7R__) || \
    defined(__ARM_ARCH_7A__)
# define ARMV7 1
#endif

#if defined(ARMV7) || \
    defined(__ARM_ARCH_6__) || \
    defined(__ARM_ARCH_6J__) || \
    defined(__ARM_ARCH_6K__) || \
    defined(__ARM_ARCH_6Z__) || \
    defined(__ARM_ARCH_6T2__) || \
    defined(__ARM_ARCH_6ZK__)
# define ARMV6 1
#endif

#if defined(ARMV6) || \
    defined(__ARM_ARCH_5T__) || \
    defined(__ARM_ARCH_5E__) || \
    defined(__ARM_ARCH_5TE__) || \
    defined(__ARM_ARCH_5TEJ__)
# define ARMV5 1
#endif

#if defined(ARMV5) || \
    defined(__ARM_ARCH_4__) || \
    defined(__ARM_ARCH_4T__)
# define ARMV4 1
#endif

#if defined(ARMV4) || \
    defined(__ARM_ARCH_3__) || \
    defined(__ARM_ARCH_3M__)
# define ARMV3 1
#endif

#if defined(ARMV3) || \
    defined(__ARM_ARCH_2__)
# define ARMV2 1
#endif

/** Architecture flags.
 *
 */
#if defined(ARMV2) || defined(__arm__) || defined(__aarch64__)
# define PLATFORM_IS_ARM
#elif defined(__i386__) || defined(_M_IX86) || defined(_X86_) || \
 defined(__amd64__) || defined(__x86_64__) || defined(_M_X64) || \
 defined(_M_AMD64)
# define PLATFORM_IS_X86
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

#  if _MSC_VER < 1900
inline int snprintf(char* str, size_t size, const char* format, ...)
{
  int written;
  va_list argv;

  va_start(argv, format);
  written = vsnprintf(str, size, format, argv);
  va_end(argv);

  return written;
}
#  endif
#else
#  define __zu "%zu"
#endif

/** Standard builtins.
 *
 */
#ifdef PLATFORM_IS_CLANG_OR_GCC
#  define __pony_popcount(X) __builtin_popcount((X))
#  define __pony_ctz(X) __builtin_ctz((X))
#  define __pony_ctzl(X) __builtin_ctzl((X))
#  define __pony_clz(X) __builtin_clz((X))
#  define __pony_clzl(X) __builtin_clzl((X))
#else
#  include <intrin.h>
#  define __pony_popcount(X) __popcnt((X))

inline uint32_t __pony_ctz(uint32_t x)
{
  DWORD i = 0;
  _BitScanForward(&i, x);
  return i;
}

#  ifdef PLATFORM_IS_ILP32
inline uint32_t __pony_ctzl(uint32_t x)
{
  DWORD i = 0;
  _BitScanForward(&i, x);
  return i;
}
#  else
inline uint64_t __pony_ctzl(uint64_t x)
{
  DWORD i = 0;
  _BitScanForward64(&i, x);
  return i;
}
#  endif

inline uint32_t __pony_clz(uint32_t x)
{
  DWORD i = 0;
  _BitScanReverse(&i, x);
  return 31 - i;
}

#  ifdef PLATFORM_IS_ILP32
inline uint32_t __pony_clzl(uint32_t x)
{
  DWORD i = 0;
  _BitScanReverse(&i, x);
  return 31 - i;
}
#  else
inline uint64_t __pony_clzl(uint64_t x)
{
  DWORD i = 0;
  _BitScanReverse64(&i, x);
  return 63 - i;
}
#  endif

#endif

/** Storage class modifiers.
 *
 */
#ifdef PLATFORM_IS_VISUAL_STUDIO
#  define __pony_spec_align__(IDENT, BYTES) \
              __declspec(align(BYTES)) IDENT
#elif defined(PLATFORM_IS_CLANG_OR_GCC)
#  define __pony_spec_align__(IDENT, BYTES) \
              IDENT __attribute__((aligned (BYTES)))
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
            ((COND) ? (THEN) : (ELSE))
#elif defined(PLATFORM_IS_CLANG_OR_GCC)
#  define EXPR_NONE ((void) 0)
#  define __pony_choose_expr(COND, THEN, ELSE) \
            __builtin_choose_expr(COND, THEN, ELSE)
#endif

#include "threads.h"
#include "paths.h"

#if defined(PLATFORM_IS_WINDOWS)
#  include "vcvars.h"
#endif

#endif

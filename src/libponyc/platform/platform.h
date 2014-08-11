#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

/** Determines the operating system.
*
*/
#if defined(__APPLE__) && defined(__MACH__)
#  define PLATFORM_IS_MACOSX
#elif defined(__linux__)
#  define PLATFORM_IS_LINUX
#elif defined(_WIN64) && defined(_MSC_VER)
#  define PLATFORM_IS_WINDOWS
#  include <Windows.h>
#  include <BaseTsd.h>
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
typedef SSIZE_T ssize_t;
typedef SIZE_T size_t;
typedef struct __int128_t { uint64_t low; int64_t high; } __int128_t;
typedef struct __uint128_t { uint64_t low; uint64_t high; } __uint128_t;
#else
#  error PLATFORM NOT SUPPORTED!
#endif

#if defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_LINUX)
# define PLATFORM_IS_POSIX_BASED
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#endif

/** Convenience macros.
 */
#define PONY_ERRNO uint32_t

/** Format specifiers.
 *
 *  SALs are only supported on Visual Studio Professional Editions. If we
 *  cannot support FORMAT_STRING, we do not validate printf-like functions.
 */
#ifndef PLATFORM_IS_WINDOWS
#  define __pony_format__(X, Y, Z) __attribute__((format \
            (X, Y, Z)))
#  define FORMAT_STRING(X) X
#else
#  define _USE_ATTRIBUTES_FOR_SAL
#  if _MSC_VER >= 1400
#    include <sal.h>
#    if _MSC_VER > 1400
#      define FORMAT_STRING(p) _Printf_format_string_ p
#    else
#      define FORMAT_STRING(p) __format_string p
#    endif
#  else
#    define FORMAT_STRING(p) p
#  endif
#  define __pony_format__(X, Y, Z)
#endif

#include "io.h"
#include "utils.h"
#include "map_file.h"
#include "primitives.h"

#endif

#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

PONY_EXTERN_C_BEGIN

#if defined(PLATFORM_IS_POSIX_BASED)
#include <limits.h>
#endif

PONY_API int pony_os_writev_max()
{
#if defined(PLATFORM_IS_POSIX_BASED)
  return IOV_MAX;
#else
  return 1;
#endif
}

PONY_EXTERN_C_END

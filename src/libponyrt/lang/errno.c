#include <platform.h>
#include "errno.h"

#ifdef PLATFORM_IS_WINDOWS
#include <winsock2.h>
#endif

PONY_EXTERN_C_BEGIN

PONY_API void pony_os_clear_errno()
{
  errno = 0;
}

PONY_API int pony_os_errno()
{
  return errno;
}

PONY_API int pony_os_socket_errno()
{
#ifdef PLATFORM_IS_WINDOWS
  return WSAGetLastError();
#else
  return errno;
#endif
}

PONY_EXTERN_C_END

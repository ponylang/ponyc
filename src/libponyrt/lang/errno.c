#include <platform.h>
#include "errno.h"

PONY_EXTERN_C_BEGIN

PONY_API void pony_os_clear_errno()
{
  errno = 0;
}

PONY_API int pony_os_errno()
{
  return errno;
}

PONY_EXTERN_C_END

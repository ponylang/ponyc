#include <platform.h>
#include <pony.h>
#include "lang.h"
#include <string.h>

PONY_EXTERN_C_BEGIN

char* os_realpath(const char* path)
{
#ifdef PLATFORM_IS_WINDOWS
  char resolved[FILENAME_MAX];

  if(GetFullPathName(path, FILENAME_MAX, resolved, NULL) == 0 ||
    GetFileAttributes(resolved) == INVALID_FILE_ATTRIBUTES)
    return NULL;
#elif defined(PLATFORM_IS_POSIX_BASED)
  char resolved[PATH_MAX];

  if(realpath(path, resolved) == NULL)
    return NULL;
#endif

  size_t len = strlen(resolved) + 1;

#ifdef PLATFORM_IS_WINDOWS
  for(; resolved[len - 1] == '\\'; --len)
    resolved[len - 1] = '\0';
#endif

  char* cstring = (char*)pony_alloc(len);
  memcpy(cstring, resolved, len);
  return cstring;
}

PONY_EXTERN_C_END

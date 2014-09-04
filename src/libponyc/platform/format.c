#include "platform.h"

#include <stdio.h>

int32_t pony_snprintf(char* str, size_t size, const char* format, ...)
{
  int32_t count;
  va_list arg;

  va_start(arg, format);
  count = pony_vsnprintf(str, size, format, arg);
  va_end(arg);

  return count;
}

int32_t pony_vsnprintf(char* str, size_t size, const char* format,
  va_list args)
{
#ifdef PLATFORM_IS_WINDOWS
  int32_t count = -1;

  if (size != 0)
    count = _vsnprintf_s(str, size, _TRUNCATE, format, args);
  if (count == -1)
    count = _vscprintf(format, args);

  return count;
#else
  return vsnprintf(str, size, format, args);
#endif
}
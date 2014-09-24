#include "platformfuns.h"
#include <assert.h>
#include <string.h>


// Report whether the named platform attribute is true
bool os_is_target(const char* attribute, bool release, bool* out_is_target)
{
  assert(attribute != NULL);
  assert(out_is_target != NULL);

  if(!strcmp(attribute, OS_LINUX_NAME))
  {
#ifdef PLATFORM_IS_LINUX
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_MACOSX_NAME))
  {
#ifdef PLATFORM_IS_MACOSX
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_WINDOWS_NAME))
  {
#ifdef PLATFORM_IS_WINDOWS
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_DEBUG_NAME))
  {
    *out_is_target = !release;
    return true;
  }

  return false;
}

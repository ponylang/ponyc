#include "platformfuns.h"
#include <assert.h>
#include <string.h>


// Report whether the named platform attribute is true
bool os_is_target(const char* attribute, bool release, bool* out_is_target)
{
  assert(attribute != NULL);
  assert(out_is_target != NULL);

  if(!strcmp(attribute, OS_FREEBSD_NAME))
  {
#ifdef PLATFORM_IS_FREEBSD
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

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

  if(!strcmp(attribute, OS_POSIX_NAME))
  {
#ifdef PLATFORM_IS_POSIX_BASED
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_X86_NAME))
  {
#ifdef PLATFORM_IS_X86
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_ARM_NAME))
  {
#ifdef PLATFORM_IS_ARM
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_LP64_NAME))
  {
#ifdef PLATFORM_IS_LP64
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_LLP64_NAME))
  {
#ifdef PLATFORM_IS_LLP64
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_ILP32_NAME))
  {
#ifdef PLATFORM_IS_ILP32
    *out_is_target = true;
#else
    *out_is_target = false;
#endif
    return true;
  }

  if(!strcmp(attribute, OS_NATIVE128_NAME))
  {
#if defined(PLATFORM_IS_ILP32) || defined(PLATFORM_IS_VISUAL_STUDIO)
    *out_is_target = false;
#else
    *out_is_target = true;
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

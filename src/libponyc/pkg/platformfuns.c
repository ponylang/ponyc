#include "platformfuns.h"
#include "../codegen/genopt.h"
#include <assert.h>
#include <string.h>


// Report whether the named platform attribute is true
bool os_is_target(const char* attribute, bool release, bool* out_is_target, pass_opt_t* options)
{
  assert(attribute != NULL);
  assert(out_is_target != NULL);
  assert(options != NULL);

  if(!strcmp(attribute, OS_FREEBSD_NAME))
  {
    *out_is_target = target_is_freebsd(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_LINUX_NAME))
  {
    *out_is_target = target_is_linux(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_MACOSX_NAME))
  {
    *out_is_target = target_is_macosx(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_WINDOWS_NAME))
  {
    *out_is_target = target_is_windows(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_POSIX_NAME))
  {
    *out_is_target = target_is_posix(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_X86_NAME))
  {
    *out_is_target = target_is_x86(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_ARM_NAME))
  {
    *out_is_target = target_is_arm(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_LP64_NAME))
  {
    *out_is_target = target_is_lp64(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_LLP64_NAME))
  {
    *out_is_target = target_is_llp64(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_ILP32_NAME))
  {
    *out_is_target = target_is_ilp32(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_NATIVE128_NAME))
  {
    *out_is_target = target_is_native128(options->triple);
    return true;
  }

  if(!strcmp(attribute, OS_DEBUG_NAME))
  {
    *out_is_target = !release;
    return true;
  }

  return false;
}

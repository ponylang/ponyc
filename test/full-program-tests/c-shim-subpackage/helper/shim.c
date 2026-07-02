#include <stdint.h>

#ifdef MAIN_VALUE
#error MAIN_VALUE leaked across packages: cdefine must stay per-package
#endif

int32_t helper_value(void)
{
  return HELPER_VALUE;
}

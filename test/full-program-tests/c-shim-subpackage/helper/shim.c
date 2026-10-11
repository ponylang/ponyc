#include <stdint.h>

#ifdef MAIN_VALUE
#error MAIN_VALUE leaked across modules: cdefine must stay per-module
#endif

int32_t helper_value(void)
{
  return HELPER_VALUE;
}

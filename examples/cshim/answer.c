#include <stdint.h>
#include "version.h"

int32_t cshim_answer(void)
{
  return CSHIM_ANSWER;
}

int32_t cshim_version(void)
{
  return CSHIM_VERSION;
}

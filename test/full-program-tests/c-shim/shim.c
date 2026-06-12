#include <stdint.h>
#include "shim_extra.h"

#ifndef SHIM_BASE
#define SHIM_BASE 0
#endif

int32_t shim_answer(void)
{
  return SHIM_BASE + SHIM_EXTRA;
}

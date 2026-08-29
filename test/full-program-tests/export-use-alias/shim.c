#include "math_export.h"

int64_t add_from_c(void* adder, int64_t x)
{
  return math_Adder_add(adder, x);
}

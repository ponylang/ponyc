#include "mylib_export.h"

int64_t add_from_c(void* adder, int64_t x)
{
  return mylib_Adder_add(adder, x);
}

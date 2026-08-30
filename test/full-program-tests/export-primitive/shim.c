#include "export-primitive_export.h"
#include <stddef.h>

int64_t call_add(int64_t x, int64_t y)
{
  return Math_add(x, y);
}

void call_noop()
{
  Math_noop();
}

int64_t call_inc(int64_t x)
{
  return Math_inc(x);
}

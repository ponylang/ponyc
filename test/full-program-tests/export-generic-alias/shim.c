#include "export-generic-alias_export.h"

int64_t call_get(void* b)
{
  return BoxedI64_get(b);
}

int64_t call_add(void* b, int64_t y)
{
  return BoxedI64_add(b, y);
}

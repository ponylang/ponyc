#include "export-methodgroup_export.h"

int64_t call_describe(void* b)
{
  return BoxedStringable_describe(b);
}

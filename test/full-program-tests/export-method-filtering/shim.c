#include "export-method-filtering_export.h"

int64_t call_public(void* f)
{
  return Filtered_public_method(f);
}

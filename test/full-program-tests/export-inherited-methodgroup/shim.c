#include "export-inherited-methodgroup_export.h"

int64_t call_i64(int64_t value)
{
  return BoxedI64_identity(value);
}

uint64_t call_u64(uint64_t value)
{
  return BoxedU64_identity(value);
}

bool check_selection(void)
{
  int64_t (*selected)(void) = BoxedI64_choice;
  void* (*fallback)(void) = BoxedU64_choice;
  (void)fallback();
  return selected() == 42;
}

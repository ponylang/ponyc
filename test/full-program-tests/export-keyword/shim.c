#include "export-keyword_export.h"

int64_t call_bar(void* foo)
{
  return Foo_bar(foo);
}

int64_t call_pass_through(void* foo, void* other)
{
  return Foo_add_other(foo, other);
}

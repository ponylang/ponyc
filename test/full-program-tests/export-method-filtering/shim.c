#include "export-method-filtering_export.h"

// These functions should NOT be exported by ponyc. If the compiler
// incorrectly exports them, the linker will fail with duplicate symbols.
void* Filtered_create(void* self, int64_t x) { return self; }
int64_t Filtered__private_method(void* self) { return -1; }
int64_t Filtered_partial_method(void* self) { return -1; }
int64_t Filtered_tuple_method(void* self) { return -1; }
int64_t Filtered_takes_tuple(void* self) { return -1; }
int64_t Filtered_bare_method(void* self, int64_t x) { return -1; }

int64_t call_public(void* f)
{
  return Filtered_public_method(f);
}

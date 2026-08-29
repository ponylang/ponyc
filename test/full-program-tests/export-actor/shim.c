#include "export-actor_export.h"

// Behaviors and constructors should NOT be exported.
// If they were, this would cause a duplicate symbol link error.
void* Counter_create(void* c) { return c; }
void Counter_increment(void* c) { (void)c; }

int64_t call_value(void* c)
{
  return Counter_value(c);
}

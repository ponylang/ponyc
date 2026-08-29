#include "export-multi-type_export.h"

// Generic method 'identity' should NOT be exported.
// If it were, this would cause a duplicate symbol link error.
void* Adder_identity(void* self) { return self; }

int64_t check_results(void* adder, void* multiplier)
{
  int64_t a1 = Adder_add(adder, 5);
  int64_t a2 = Adder_add_twice(adder, 3);

  int64_t m1 = Multiplier_mul(multiplier, 7);
  int64_t m2 = Multiplier_mul_add(multiplier, 4, 2);

  // a1 = 10+5 = 15, a2 = 10+3+3 = 16, m1 = 3*7 = 21, m2 = 3*4+2 = 14
  if(a1 == 15 && a2 == 16 && m1 == 21 && m2 == 14)
    return 0;
  return 1;
}

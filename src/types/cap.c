#include "cap.h"

bool cap_sub(cap_id a, cap_id b)
{
  return (a >= K_ISO) && (b >= a) && (a != K_MUT || b != K_IMM);
}

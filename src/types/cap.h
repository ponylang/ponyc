#ifndef CAP_H
#define CAP_H

#include <stdbool.h>

typedef enum
{
  K_NEW,
  K_BE,
  K_ISO,
  K_TRN,
  K_MUT,
  K_IMM,
  K_BOX,
  K_TAG
} cap_id;

bool cap_sub(cap_id a, cap_id b);

#endif

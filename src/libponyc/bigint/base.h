#ifndef BIGINT_BASE_H
#define BIGINT_BASE_H

#include <stdint.h>

typedef enum cmp_t 
{
  less = -1,
  equal = 0,
  greater = 1,
  invalid = UINT32_MAX
} cmp_t;

typedef enum sign_t
{
  negative = -1,
  zero = 0,
  positive = 1,
} sign_t;

#endif
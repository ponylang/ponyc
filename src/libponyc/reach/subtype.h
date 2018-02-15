#ifndef REACH_SUBTYPE_H
#define REACH_SUBTYPE_H

#include "reach.h"

PONY_EXTERN_C_BEGIN

enum subtype_kind_t
{
  SUBTYPE_KIND_NONE,
  SUBTYPE_KIND_NUMERIC = 1 << 0,
  SUBTYPE_KIND_TUPLE = 1 << 1,
  SUBTYPE_KIND_UNBOXED = 1 << 2,

  SUBTYPE_KIND_BOXED = SUBTYPE_KIND_NUMERIC | SUBTYPE_KIND_TUPLE,
  SUBTYPE_KIND_ALL = SUBTYPE_KIND_BOXED | SUBTYPE_KIND_UNBOXED
};

int subtype_kind(reach_type_t* type);

int subtype_kind_overlap(reach_type_t* left, reach_type_t* right);

PONY_EXTERN_C_END

#endif

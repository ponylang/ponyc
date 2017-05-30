#include "subtype.h"
#include <ponyassert.h>

int subtype_kind(reach_type_t* type)
{
  int subtypes = SUBTYPE_KIND_NONE;

  size_t i = HASHMAP_BEGIN;
  reach_type_t* sub;

  while((sub = reach_type_cache_next(&type->subtypes, &i)) != NULL)
  {
    if(sub->can_be_boxed)
    {
      if(type->underlying == TK_PRIMITIVE)
        subtypes |= SUBTYPE_KIND_NUMERIC;
      else
        subtypes |= SUBTYPE_KIND_TUPLE;
    } else {
      subtypes |= SUBTYPE_KIND_UNBOXED;
    }

    if(subtypes == SUBTYPE_KIND_ALL)
      return subtypes;
  }

  return subtypes;
}

int subtype_kind_overlap(reach_type_t* left, reach_type_t* right)
{
  int subtypes = SUBTYPE_KIND_NONE;

  size_t i = HASHMAP_BEGIN;
  reach_type_t* sub_left;

  while((sub_left = reach_type_cache_next(&left->subtypes, &i)) != NULL)
  {
    if(!sub_left->can_be_boxed)
    {
      subtypes |= SUBTYPE_KIND_UNBOXED;
      if(subtypes == SUBTYPE_KIND_ALL)
        return subtypes;

      continue;
    }

    reach_type_t k;
    k.name = sub_left->name;
    size_t j = HASHMAP_UNKNOWN;
    reach_type_t* sub_right = reach_type_cache_get(&right->subtypes, &k, &j);

    if(sub_right != NULL)
    {
      pony_assert(sub_left == sub_right);

      if(sub_left->underlying == TK_PRIMITIVE)
        subtypes |= SUBTYPE_KIND_NUMERIC;
      else
        subtypes |= SUBTYPE_KIND_TUPLE;

      if(subtypes == SUBTYPE_KIND_ALL)
        return subtypes;
    }
  }

  return subtypes;
}

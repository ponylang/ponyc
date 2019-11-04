#include "subtype.h"
#include <ponyassert.h>

int subtype_kind(reach_type_t* type)
{
  pony_assert(type != NULL);

  int subtypes = SUBTYPE_KIND_NONE;

  size_t i = HASHMAP_BEGIN;
  reach_type_t* sub;

  while((sub = reach_type_cache_next(&type->subtypes, &i)) != NULL)
  {
    if(sub->can_be_boxed)
    {
      if(sub->underlying == TK_PRIMITIVE)
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
  pony_assert(left != NULL);

  if(right == NULL)
    return subtype_kind(left);

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

    if(sub_left->underlying != TK_TUPLETYPE)
    {
      reach_type_t k;
      k.name = sub_left->name;
      size_t j = HASHMAP_UNKNOWN;
      reach_type_t* sub_right = reach_type_cache_get(&right->subtypes, &k, &j);

      if(sub_right != NULL)
      {
        pony_assert(sub_left == sub_right);

        if(sub_left->underlying == TK_PRIMITIVE)
          subtypes |= SUBTYPE_KIND_NUMERIC;

        if(subtypes == SUBTYPE_KIND_ALL)
          return subtypes;
      }
    } else if((subtypes & SUBTYPE_KIND_TUPLE) == 0) {
      size_t cardinality = ast_childcount(sub_left->ast_cap);
      size_t j = HASHMAP_UNKNOWN;
      reach_type_t* sub_right;

      while((sub_right = reach_type_cache_next(&right->subtypes, &j)) != NULL)
      {
        if((sub_right->underlying == TK_TUPLETYPE) &&
          (ast_childcount(sub_right->ast_cap) == cardinality))
        {
          subtypes |= SUBTYPE_KIND_TUPLE;

          if(subtypes == SUBTYPE_KIND_ALL)
            return subtypes;

          break;
        }
      }
    }
  }

  return subtypes;
}

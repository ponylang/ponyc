#include "alias.h"
#include <assert.h>

ast_t* alias(ast_t* type)
{
  if(ast_id(type) == TK_ERROR)
    return type;

  assert(ast_id(type) == TK_TYPEDEF);
  ast_t* child = ast_child(type);
  ast_t* cap = ast_sibling(child);
  ast_t* ephemeral = ast_sibling(cap);

  switch(ast_id(child))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      // alias each side
      ast_t* r_type = ast_dup(type);
      child = ast_child(r_type);

      ast_t* left = ast_child(child);
      ast_t* right = ast_sibling(left);

      ast_swap(left, alias(left));
      ast_swap(right, alias(right));

      return r_type;
    }

    case TK_TUPLETYPE:
    case TK_NOMINAL:
    case TK_STRUCTURAL:
    {
      if(ast_id(ephemeral) == TK_HAT)
      {
        // ephemeral capability becomes non-ephemeral
        ast_t* r_type = ast_dup(type);
        ephemeral = ast_childidx(r_type, 2);
        ast_swap(ephemeral, ast_from(r_type, TK_NONE));
        return r_type;
      }

      // TODO: alias non-ephemeral
      // simple if the cap doesn't include viewpoint adaptation
      // otherwise it's a pain
      break;
    }

    default: {}
  }

  assert(0);
  return NULL;
}

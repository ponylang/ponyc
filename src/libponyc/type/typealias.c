#include "typealias.h"
#include "cap.h"
#include "reify.h"
#include "../ast/token.h"
#include "ponyassert.h"

static void apply_cap_to_type(ast_t* type, token_id tcap, token_id teph)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      for(ast_t* child = ast_child(type);
        child != NULL;
        child = ast_sibling(child))
      {
        apply_cap_to_type(child, tcap, teph);
      }

      return;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    case TK_TYPEALIASREF:
    {
      ast_t* r_cap = cap_fetch(type);

      if(tcap != TK_NONE)
        ast_setid(r_cap, tcap);

      if(teph != TK_NONE)
      {
        ast_t* r_eph = ast_sibling(r_cap);
        ast_setid(r_eph, teph);
      }

      return;
    }

    case TK_ARROW:
      apply_cap_to_type(ast_childidx(type, 1), tcap, teph);
      return;

    default: {}
  }
}

ast_t* typealias_unfold(ast_t* typealiasref)
{
  pony_assert(ast_id(typealiasref) == TK_TYPEALIASREF);

  ast_t* def = (ast_t*)ast_data(typealiasref);
  pony_assert(def != NULL);
  pony_assert(ast_id(def) == TK_TYPE);

  AST_GET_CHILDREN(typealiasref, id, typeargs, cap, eph);
  AST_GET_CHILDREN(def, alias_id, typeparams, def_cap, provides);

  ast_t* alias = ast_child(provides);
  pony_assert(alias != NULL);

  // Reify the alias definition with our type arguments.
  ast_t* r_alias = reify(alias, typeparams, typeargs, NULL, true);

  if(r_alias == NULL)
    return NULL;

  // Apply our cap and ephemeral to the result.
  token_id tcap = ast_id(cap);
  token_id teph = ast_id(eph);

  if((tcap != TK_NONE) || (teph != TK_NONE))
    apply_cap_to_type(r_alias, tcap, teph);

  // If the result is itself a TK_TYPEALIASREF (a chained alias like
  // `type A is B; type B is (U64, U64)`), unfold transitively. This is
  // bounded by the length of the alias chain: pass/names.c's
  // names_resolvealias uses AST_FLAG_RECURSE_1 to detect and reject cyclic
  // alias defs before any TK_TYPEALIASREF is emitted, so the chain is a
  // finite DAG. Callers rely on the returned head not being a
  // TK_TYPEALIASREF (see typealias.h).
  if(ast_id(r_alias) == TK_TYPEALIASREF)
  {
    ast_t* next = typealias_unfold(r_alias);
    ast_free_unattached(r_alias);
    return next;
  }

  return r_alias;
}

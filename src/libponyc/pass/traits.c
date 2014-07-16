#include "traits.h"
#include "../ast/token.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../type/assemble.h"
#include <assert.h>

typedef enum
{
  TRAITS_INITIAL = 0,
  TRAITS_IN_PROGRESS,
  TRAITS_DONE
} trait_state_t;

static bool attach_method(ast_t* type, ast_t* method)
{
  if((ast_id(type) == TK_CLASS) && (ast_id(method) == TK_BE))
  {
    ast_error(type, "a class can't have traits that have behaviours");
    return false;
  }

  ast_t* members = ast_childidx(type, 4);

  // see if we have an existing method with this name
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* existing = ast_get(type, name);

  if(existing != NULL)
  {
    // check our version is a subtype of the supplied version
    if(!is_subtype(existing, method))
    {
      ast_error(existing, "existing method is not a subtype of trait method");
      ast_error(method, "trait method is here");
      return false;
    }

    // if existing has no implementation, accept this implementation
    ast_t* existing_impl = ast_childidx(existing, 6);
    ast_t* impl = ast_childidx(method, 6);

    if((ast_id(existing_impl) == TK_NONE) && (ast_id(impl) != TK_NONE))
      ast_replace(&existing_impl, impl);

    // TODO: what if a new method is a subtype of an existing method?
    // if the existing method came from a trait, should we accept the new one?

    return true;
  }

  // insert into our members
  method = ast_append(members, method);

  // add to our scope
  ast_set(type, name, method);

  return true;
}

static bool attach_traits(ast_t* def)
{
  trait_state_t state = (trait_state_t)ast_data(def);

  switch(state)
  {
    case TRAITS_INITIAL:
      ast_setdata(def, (void*)TRAITS_IN_PROGRESS);
      break;

    case TRAITS_IN_PROGRESS:
      ast_error(def, "traits can't be recursive");
      return false;

    case TRAITS_DONE:
      return true;

    default:
      assert(0);
      return false;
  }

  ast_t* traits = ast_childidx(def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    ast_t* trait_def = ast_data(trait);

    if(ast_id(trait_def) != TK_TRAIT)
    {
      ast_error(trait, "must be a trait");
      return false;
    }

    if(!attach_traits(trait_def))
      return false;

    ast_t* typeparams = ast_childidx(trait_def, 1);
    ast_t* typeargs = ast_childidx(trait, 2);
    ast_t* members = ast_childidx(trait_def, 4);
    ast_t* member = ast_child(members);

    while(member != NULL)
    {
      switch(ast_id(member))
      {
        case TK_NEW:
        case TK_FUN:
        case TK_BE:
        {
          // reify the method with the type parameters from trait_def
          // and the reified type arguments from r_trait.
          ast_t* r_member = reify(member, typeparams, typeargs);
          bool ok = attach_method(def, r_member);
          ast_free_unattached(r_member);

          if(!ok)
            return false;

          break;
        }

        default:
          assert(0);
          return false;
      }

      member = ast_sibling(member);
    }

    trait = ast_sibling(trait);
  }

  ast_setdata(def, (void*)TRAITS_DONE);
  return true;
}

static bool have_impl(ast_t* ast)
{
  ast_t* members = ast_childidx(ast, 4);
  ast_t* member = ast_child(members);
  bool ret = true;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        ast_t* impl = ast_childidx(member, 6);

        if(ast_id(impl) == TK_NONE)
        {
          ast_t* id = ast_childidx(member, 1);
          const char* name = ast_name(id);
          ast_error(ast, "method '%s' has no implementation", name);
          ret = false;
        }
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return ret;
}

ast_result_t pass_traits(ast_t** astp)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_TRAIT:
      if(!attach_traits(ast))
        return AST_ERROR;
      break;

    case TK_CLASS:
    case TK_ACTOR:
      if(!attach_traits(ast) || !have_impl(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}

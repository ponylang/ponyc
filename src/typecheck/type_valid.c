#include "type_valid.h"
#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

typedef enum
{
  TYPE_INITIAL = 0,
  TYPE_TRAITS_IN_PROGRESS,
  TYPE_TRAITS_DONE
} type_state_t;

static bool attach_method(ast_t* type, ast_t* method)
{
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

    // TODO: if existing has no implementation, accept this implementation
    // what if a new method is a subtype of an existing method?
    // if the existing method came from a trait, should we accept the new one?

    return true;
  }

  // insert into our members
  ast_t* r_method = ast_dup(method);
  ast_append(members, r_method);

  // add to our scope
  ast_set(type, name, r_method);

  return true;
}

static bool attach_traits(ast_t* def)
{
  type_state_t state = (type_state_t)ast_data(def);

  switch(state)
  {
    case TYPE_INITIAL:
      ast_attach(def, (void*)TYPE_TRAITS_IN_PROGRESS);
      break;

    case TYPE_TRAITS_IN_PROGRESS:
      ast_error(def, "traits cannot be recursive");
      return false;

    case TYPE_TRAITS_DONE:
      return true;
  }

  ast_t* traits = ast_childidx(def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    ast_t* nominal = ast_child(trait);

    if(ast_id(nominal) != TK_NOMINAL)
    {
      ast_error(nominal, "traits must be nominal types");
      return false;
    }

    ast_t* trait_def = nominal_def(nominal);

    if(ast_id(trait_def) != TK_TRAIT)
    {
      ast_error(nominal, "must be a trait");
      return false;
    }

    if(!attach_traits(trait_def))
      return false;

    ast_t* typeparams = ast_childidx(trait_def, 1);
    ast_t* typeargs = ast_childidx(nominal, 1);
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
          // and the reified type arguments from r_trait
          ast_t* r_member = reify(member, typeparams, typeargs);
          bool ok = attach_method(def, r_member);

          if(r_member != member)
            ast_free(r_member);

          if(!ok)
            return false;

          break;
        }

        default: assert(0);
      }

      member = ast_sibling(member);
    }

    trait = ast_sibling(trait);
  }

  ast_attach(def, (void*)TYPE_TRAITS_DONE);
  return true;
}

static bool check_constraints(ast_t* type, ast_t* typeargs)
{
  if(ast_id(type) == TK_TYPEPARAM)
  {
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs, "type parameters cannot have type arguments");
      return false;
    }

    return true;
  }

  // reify the type parameters with the typeargs
  ast_t* typeparams = ast_childidx(type, 1);
  ast_t* r_typeparams = reify(typeparams, typeparams, typeargs);

  if(r_typeparams == NULL)
    return false;

  ast_t* typeparam = ast_child(r_typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    ast_t* constraint = ast_childidx(typeparam, 1);

    if((ast_id(constraint) != TK_NONE) && !is_subtype(typearg, constraint))
    {
      ast_error(typearg, "type argument is outside its constraint");
      ast_error(typeparam, "constraint is here");

      if(r_typeparams != typeparams)
        ast_free(r_typeparams);

      return false;
    }

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  if(r_typeparams != typeparams)
    ast_free(r_typeparams);

  return true;
}

static bool replace_alias(ast_t* def, ast_t* nominal, ast_t* typeargs)
{
  // see if this is a type alias
  if(ast_id(def) != TK_TYPE)
    return true;

  ast_t* alias_list = ast_childidx(def, 3);

  // if this type alias has no alias list, we're done
  // so type None stays as None
  // but type Bool is (True | False) must be replaced with (True | False)
  if(ast_id(alias_list) == TK_NONE)
    return true;

  // we should have a single TYPEDEF as our alias
  assert(ast_id(alias_list) == TK_TYPES);
  ast_t* alias = ast_child(alias_list);
  assert(ast_sibling(alias) == NULL);

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* r_alias = reify(alias, typeparams, typeargs);

  if(r_alias == NULL)
    return false;

  if(r_alias == alias)
    r_alias = ast_dup(alias);

  // the thing being swapped in must be a TYPEDEF
  assert(ast_id(r_alias) == TK_TYPEDEF);

  // the thing being swapped out must be a TYPEDEF
  ast_t* type_def = ast_parent(nominal);
  assert(ast_id(type_def) == TK_TYPEDEF);

  // use the new ast and free the old one
  ast_swap(type_def, r_alias);
  ast_free(type_def);

  return true;
}

/**
 * This checks that all explicit types are valid.
 */
bool type_valid(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
    {
      // TODO: capability
      ast_t* def = nominal_def(ast);

      if(def == NULL)
        return false;

      // make sure our typeargs are subtypes of our constraints
      ast_t* typeargs = ast_childidx(ast, 1);

      if(!check_constraints(def, typeargs))
        return false;

      if(!replace_alias(def, ast, typeargs))
        return false;

      return true;
    }

    case TK_ASSIGN:
      // TODO: check for syntactic sugar for update
      break;

    case TK_RECOVER:
      // TODO: remove things from following scope
      break;

    case TK_FOR:
      // TODO: syntactic sugar for a while loop
      break;

    default: {}
  }

  return true;
}

bool type_traits(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      return attach_traits(ast);

    default: {}
  }

  return true;
}

#include "valid.h"
#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

static bool is_typeparam(ast_t* typeparam, ast_t* typearg)
{
  assert(ast_id(typearg) == TK_TYPEDEF);
  ast_t* sub = ast_child(typearg);

  if(ast_id(sub) != TK_NOMINAL)
    return false;

  ast_t* def = nominal_def(typearg, sub);
  return def == typeparam;
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

  ast_t* r_typeparam = ast_child(r_typeparams);
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((r_typeparam != NULL) && (typearg != NULL))
  {
    ast_t* constraint = ast_childidx(r_typeparam, 1);

    // compare the typearg to the typeparam and constraint
    if(!is_typeparam(typeparam, typearg) &&
      (ast_id(constraint) != TK_NONE) &&
      !is_subtype(type, typearg, constraint)
      )
    {
      ast_error(typearg, "type argument is outside its constraint");
      ast_error(typeparam, "constraint is here");

      if(r_typeparams != typeparams)
        ast_free(r_typeparams);

      return false;
    }

    r_typeparam = ast_sibling(r_typeparam);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  ast_free_unattached(r_typeparams);
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

  // extract from the typedef
  assert(ast_id(r_alias) == TK_TYPEDEF);
  ast_t* alias_def = ast_child(r_alias);

  // use the alias definition and free the previous nominal type
  ast_swap(nominal, alias_def);
  ast_free(nominal);
  ast_free_unattached(r_alias);

  return true;
}

static bool valid_typedef(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* cap = ast_sibling(child);
  ast_t* ephemeral = ast_sibling(cap);
  ast_t* error = ast_sibling(ephemeral);
  ast_t* next = ast_sibling(error);
  token_id t = TK_REF;

  if(ast_id(next) != TK_NONE)
  {
    switch(ast_id(child))
    {
      case TK_THIS:
      case TK_NOMINAL:
        break;

      default:
        ast_error(child, "only type parameters and 'this' can be viewpoints");
        return false;
    }

    if(ast_id(cap) != TK_NONE)
    {
      ast_error(cap, "a viewpoint cannot specify a capability");
      return false;
    }

    if(ast_id(ephemeral) != TK_NONE)
    {
      ast_error(ephemeral, "a viewpoint cannot be ephemeral");
      return false;
    }
  }

  switch(ast_id(child))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      if(ast_id(cap) != TK_NONE)
      {
        ast_error(cap, "can't specify a capability on a union/isect type");
        return false;
      }

      if(ast_id(ephemeral) != TK_NONE)
      {
        ast_error(ephemeral, "can't mark a union/isect type as ephemeral");
        return false;
      }

      return true;
    }

    case TK_TUPLETYPE:
    case TK_STRUCTURAL:
      // default capability for tuples and structural types is ref
      t = TK_REF;
      break;

    case TK_NOMINAL:
    {
      // default capability for a nominal type
      ast_t* def = nominal_def(ast, child);
      ast_t* defcap = ast_childidx(def, 2);
      t = ast_id(defcap);

      if((ast_id(next) != TK_NONE) && (ast_id(def) != TK_TYPEPARAM))
      {
        ast_error(ast, "only type parameters can be viewpoints");
        return false;
      }

      if(t == TK_NONE)
      {
        // no default capability specified
        switch(ast_id(def))
        {
          case TK_TYPE:
            // a type is a val
            t = TK_VAL;
            break;

          case TK_TRAIT:
          case TK_CLASS:
            // a trait or class is a ref
            t = TK_REF;
            break;

          case TK_ACTOR:
            // an actor is a tag
            t = TK_TAG;
            break;

          case TK_TYPEPARAM:
          {
            // don't set the capability
            // TODO:
            return true;
          }

          default:
            assert(0);
            return false;
        }
      }
      break;
    }

    case TK_THIS:
    {
      // must be the first thing in viewpoint adaptation
      if(ast_id(ast_parent(ast)) == TK_TYPEDEF)
      {
        ast_error(ast, "when using 'this' for viewpoint it must come first");
        return false;
      }

      if(ast_id(next) == TK_NONE)
      {
        ast_error(ast, "'this' in a type can only be used for viewpoint");
        return false;
      }
      break;
    }

    default:
      assert(0);
      return false;
  }

  if(ast_id(cap) == TK_NONE)
  {
    // TODO: if we are a constraint, should we use tag for traits, classes and
    // structural types instead?
    ast_t* newcap = ast_from(ast, t);
    ast_swap(cap, newcap);
    ast_free(cap);
  }

  if((ast_id(ephemeral) == TK_HAT) && (ast_enclosing_type(ast) == NULL))
  {
    ast_error(ephemeral, "ephemeral types can only be function return types");
    return false;
  }

  return true;
}

/**
 * This checks that all explicit types are valid. It also attaches the
 * definition ast node to each nominal type, and replaces type aliases with
 * their reified expansion.
 */
ast_result_t type_valid(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!valid_nominal(ast, ast))
        return AST_ERROR;
      break;

    case TK_TYPEDEF:
      if(!valid_typedef(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}

bool valid_nominal(ast_t* scope, ast_t* nominal)
{
  // TODO: capability
  ast_t* def = nominal_def(scope, nominal);

  if(def == NULL)
    return false;

  // make sure our typeargs are subtypes of our constraints
  ast_t* typeargs = ast_childidx(nominal, 2);

  if(!check_constraints(def, typeargs))
    return false;

  if(!replace_alias(def, nominal, typeargs))
    return false;

  return true;
}

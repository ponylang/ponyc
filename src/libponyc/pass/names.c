#include "names.h"
#include "../pkg/package.h"
#include <assert.h>

typedef enum
{
  TYPEALIAS_INITIAL = 0,
  TYPEALIAS_IN_PROGRESS,
  TYPEALIAS_DONE
} typealias_state_t;

static void names_applycap_index(ast_t* ast, ast_t* cap, ast_t* ephemeral,
  int index)
{
  ast_t* a_cap = ast_childidx(ast, index);
  ast_t* a_ephemeral = ast_sibling(a_cap);

  if(ast_id(cap) != TK_NONE)
    ast_replace(&a_cap, cap);

  if(ast_id(ephemeral) != TK_NONE)
    ast_replace(&a_ephemeral, ephemeral);
}

static bool names_applycap(ast_t* ast, ast_t* cap, ast_t* ephemeral)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      if(ast_id(cap) != TK_NONE)
      {
        ast_error(cap,
          "can't specify a capability for an alias to a type expression");
        return false;
      }

      if(ast_id(ephemeral) != TK_NONE)
      {
        ast_error(ephemeral,
          "can't specify ephemerality for an alias to a type expression");
        return false;
      }

      return true;
    }

    case TK_NOMINAL:
      names_applycap_index(ast, cap, ephemeral, 3);
      return true;

    case TK_STRUCTURAL:
      names_applycap_index(ast, cap, ephemeral, 1);
      return true;

    case TK_ARROW:
      return names_applycap(ast_childidx(ast, 1), cap, ephemeral);

    default: {}
  }

  assert(0);
  return false;
}

static bool names_resolvealias(ast_t* ast)
{
  typealias_state_t state = (typealias_state_t)ast_data(ast);

  switch(state)
  {
    case TYPEALIAS_INITIAL:
      ast_setdata(ast, (void*)TYPEALIAS_IN_PROGRESS);
      break;

    case TYPEALIAS_IN_PROGRESS:
      ast_error(ast, "type aliases can't be recursive");
      return false;

    case TYPEALIAS_DONE:
      return true;

    default:
      assert(0);
      return false;
  }

  if(ast_visit(&ast, NULL, pass_names) != AST_OK)
    return false;

  ast_setdata(ast, (void*)TYPEALIAS_DONE);
  return true;
}

static bool names_typealias(ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;

  // type aliases can't have type arguments
  ast_t* typeargs = ast_childidx(ast, 2);

  if(ast_id(typeargs) != TK_NONE)
  {
    ast_error(typeargs, "type aliases can't have type arguments");
    return false;
  }

  // make sure the alias is resolved
  ast_t* alias = ast_childidx(def, 3);

  if(!names_resolvealias(alias))
    return false;

  // apply our cap and ephemeral to the result
  ast_t* cap = ast_childidx(ast, 3);
  ast_t* ephemeral = ast_sibling(cap);
  alias = ast_dup(ast_child(alias));

  if(!names_applycap(alias, cap, ephemeral))
  {
    ast_free_unattached(alias);
    return false;
  }

  // replace this with the alias
  ast_replace(astp, alias);
  return true;
}

static bool names_typeparam(ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;
  ast_t* package = ast_child(ast);
  ast_t* type = ast_sibling(package);
  ast_t* typeargs = ast_sibling(type);
  ast_t* cap = ast_sibling(typeargs);
  ast_t* ephemeral = ast_sibling(cap);

  assert(ast_id(package) == TK_NONE);

  if(ast_id(typeargs) != TK_NONE)
  {
    ast_error(typeargs, "can't qualify a type parameter with type arguments");
    return false;
  }

  // change to a typeparamref
  ast_t* typeparamref = ast_from(ast, TK_TYPEPARAMREF);
  ast_add(typeparamref, ephemeral);
  ast_add(typeparamref, cap);
  ast_add(typeparamref, type);
  ast_setdata(typeparamref, def);
  ast_replace(astp, typeparamref);

  return true;
}

static bool names_type(ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;
  ast_t* package = ast_child(ast);
  ast_t* cap = ast_childidx(ast, 3);
  ast_t* defcap;

  // a nominal constraint without a capability is set to tag, otherwise to
  // the default capability for the type. if the nominal type in a
  // constraint appears inside a structural type, use the default cap for
  // the type, not tag.
  if(ast_id(cap) == TK_NONE)
  {
    if(ast_enclosing_constraint(ast) != NULL)
      defcap = ast_from(cap, TK_TAG);
    else
      defcap = ast_childidx(def, 2);

    ast_replace(&cap, defcap);
  }

  // keep the actual package id
  ast_replace(&package, package_id(ast_nearest(def, TK_PACKAGE)));

  // store our definition for later use
  ast_setdata(ast, def);
  return true;
}

bool names_nominal(ast_t* scope, ast_t** astp)
{
  ast_t* ast = *astp;

  if(ast_data(ast) != NULL)
    return true;

  ast_t* package = ast_child(ast);
  ast_t* type = ast_sibling(package);

  // find our actual package
  if(ast_id(package) != TK_NONE)
  {
    const char* name = ast_name(package);
    scope = ast_get(scope, name);

    if((scope == NULL) || (ast_id(scope) != TK_PACKAGE))
    {
      ast_error(package, "can't find package '%s'", name);
      return false;
    }
  }

  // find our definition
  const char* name = ast_name(type);
  ast_t* def = ast_get(scope, name);

  if(def == NULL)
  {
    ast_error(type, "can't find definition of '%s'", name);
    return false;
  }

  switch(ast_id(def))
  {
    case TK_TYPE:
      return names_typealias(astp, def);

    case TK_TYPEPARAM:
      return names_typeparam(astp, def);

    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      return names_type(astp, def);

    default: {}
  }

  ast_error(type, "definition of '%s' is not a type", name);
  return false;
}

static bool names_arrow(ast_t* ast)
{
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_THISTYPE:
    case TK_TYPEPARAMREF:
      return true;

    default: {}
  }

  ast_error(left, "only type parameters and 'this' can be viewpoints");
  return false;
}

ast_result_t pass_names(ast_t** astp)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!names_nominal(ast, astp))
        return AST_ERROR;
      break;

    case TK_ARROW:
      if(!names_arrow(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}

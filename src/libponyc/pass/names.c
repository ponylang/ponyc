#include "names.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include <assert.h>

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

static bool names_resolvealias(ast_t* def, ast_t* type)
{
  ast_state_t state = (ast_state_t)((uint64_t)ast_data(def));

  switch(state)
  {
    case AST_STATE_INITIAL:
      ast_setdata(def, (void*)AST_STATE_INPROGRESS);
      break;

    case AST_STATE_INPROGRESS:
      ast_error(def, "type aliases can't be recursive");
      return false;

    case AST_STATE_DONE:
      return true;

    default:
      assert(0);
      return false;
  }

  if(ast_visit(&type, NULL, pass_names, NULL) != AST_OK)
    return false;

  ast_setdata(def, (void*)AST_STATE_DONE);
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
  ast_t* alias = ast_childidx(def, 1);

  if(!names_resolvealias(def, alias))
    return false;

  // apply our cap and ephemeral to the result
  ast_t* cap = ast_childidx(ast, 3);
  ast_t* ephemeral = ast_sibling(cap);
  alias = ast_dup(alias);
  //alias = ast_dup(ast_child(alias));

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
    if((ast_id(def) == TK_PRIMITIVE) || (ast_enclosing_constraint(ast) != NULL))
      defcap = ast_from(cap, TK_TAG);
    else
      defcap = ast_childidx(def, 2);

    ast_replace(&cap, defcap);
  } else if(ast_id(def) == TK_PRIMITIVE) {
    if(ast_id(cap) != TK_TAG)
    {
      ast_error(ast, "primitives must always be tag");
      return false;
    }
  }

  // keep the actual package id
  ast_replace(&package, package_id(def));

  // store our definition for later use
  ast_setdata(ast, def);
  return true;
}

bool names_nominal(ast_t* scope, ast_t** astp)
{
  ast_t* ast = *astp;

  if(ast_data(ast) != NULL)
    return true;

  AST_GET_CHILDREN(ast, package, type);
  bool local_package;

  // find our actual package
  if(ast_id(package) != TK_NONE)
  {
    const char* name = ast_name(package);

    if(name[0] == '$')
      scope = ast_get(ast_nearest(scope, TK_PROGRAM), name, NULL);
    else
      scope = ast_get(scope, name, NULL);

    if((scope == NULL) || (ast_id(scope) != TK_PACKAGE))
    {
      ast_error(package, "can't find package '%s'", name);
      return false;
    }

    local_package = scope == ast_nearest(ast, TK_PACKAGE);
  } else {
    local_package = true;
  }

  // find our definition
  const char* name = ast_name(type);
  ast_t* def = ast_get(scope, name, NULL);

  if(def == NULL)
  {
    ast_error(type, "can't find definition of '%s'", name);
    return false;
  }

  if(!local_package && (name[0] == '_'))
  {
    ast_error(type, "can't access a private type from another package");
    return false;
  }

  switch(ast_id(def))
  {
    case TK_TYPE:
      return names_typealias(astp, def);

    case TK_TYPEPARAM:
      return names_typeparam(astp, def);

    case TK_TRAIT:
    case TK_PRIMITIVE:
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

ast_result_t pass_names(ast_t** astp, pass_opt_t* options)
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

static bool flatten_union(ast_t** astp)
{
  // compact union types
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);
  ast_t* type = NULL;

  while(child != NULL)
  {
    type = type_union(type, child);
    child = ast_sibling(child);
  }

  ast_replace(astp, type);
  return true;
}

static bool flatten_isect(ast_t** astp)
{
  // compact isect types
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);
  ast_t* type = NULL;

  while(child != NULL)
  {
    type = type_isect(type, child);
    child = ast_sibling(child);
  }

  ast_replace(astp, type);
  return true;
}

ast_result_t pass_flatten(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
      if(!flatten_union(astp))
        return AST_ERROR;
      break;

    case TK_ISECTTYPE:
      if(!flatten_isect(astp))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}

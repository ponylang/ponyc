#include "reify.h"
#include "typechecker.h"
#include <assert.h>

static bool reify_one(ast_t* ast, ast_t* id, ast_t* typearg);

static bool reify_fun(ast_t* fun, ast_t* id, ast_t* typearg)
{
  assert(
    (ast_id(fun) == TK_NEW) ||
    (ast_id(fun) == TK_BE) ||
    (ast_id(fun) == TK_FUN)
    );

  ast_t* typeparams = ast_childidx(fun, 2);
  ast_t* params_or_types = ast_sibling(typeparams);
  ast_t* result = ast_sibling(params_or_types);

  return reify_one(typeparams, id, typearg) &&
    reify_one(params_or_types, id, typearg) &&
    reify_one(result, id, typearg);
}

static bool reify_typedef(ast_t* type, ast_t* id, ast_t* typearg)
{
  assert(ast_id(type) == TK_TYPEDEF);
  assert(ast_id(id) == TK_ID);
  assert(ast_id(typearg) == TK_TYPEDEF);

  // make sure typearg has nominal_def calculated in the right scope
  ast_t* sub_typearg = ast_child(typearg);

  if(ast_id(sub_typearg) == TK_NOMINAL)
    nominal_def(typearg, sub_typearg);

  ast_t* sub_type = ast_child(type);

  switch(ast_id(sub_type))
  {
    case TK_NOMINAL:
    {
      ast_t* package = ast_child(sub_type);
      ast_t* type_name = ast_sibling(package);
      ast_t* typeargs = ast_sibling(type_name);

      if((ast_id(package) == TK_NONE) && (ast_name(type_name) == ast_name(id)))
      {
        // we should not have typeargs
        if(ast_id(typeargs) != TK_NONE)
          return false;

        // TODO: when reifying, need to preserve viewpoint adaptation
        // also cap and ephemeral
        ast_swap(type, ast_dup(typearg));
        ast_free(type);
      } else {
        if(!reify_one(typeargs, id, typearg))
          return false;
      }

      return true;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    case TK_STRUCTURAL:
      return reify_one(sub_type, id, typearg);

    default: {}
  }

  assert(0);
  return false;
}

static bool reify_params(ast_t* params, ast_t* id, ast_t* typearg)
{
  assert(
    (ast_id(params) == TK_TYPEPARAMS) ||
    (ast_id(params) == TK_PARAMS)
    );
  assert(ast_id(id) == TK_ID);
  assert(ast_id(typearg) == TK_TYPEDEF);

  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* name = ast_child(param);
    ast_t* type = ast_sibling(name);

    if(!reify_one(type, id, typearg))
      return false;

    param = ast_sibling(param);
  }

  return true;
}

static bool reify_one(ast_t* ast, ast_t* id, ast_t* typearg)
{
  switch(ast_id(ast))
  {
    case TK_NONE:
      return true;

    case TK_TYPEPARAMS:
    case TK_PARAMS:
      return reify_params(ast, id, typearg);

    case TK_TYPEARGS:
    case TK_TYPES:
    case TK_STRUCTURAL:
    {
      // all of these are simple containers of things that get reified
      // iterate through and reify each one
      ast_t* sub = ast_child(ast);

      while(sub != NULL)
      {
        if(!reify_one(sub, id, typearg))
          return false;

        sub = ast_sibling(sub);
      }

      return true;
    }

    case TK_TYPEDEF:
      return reify_typedef(ast, id, typearg);

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      return reify_fun(ast, id, typearg);

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* left = ast_child(ast);
      ast_t* right = ast_sibling(left);
      return reify_one(left, id, typearg) &&
        reify_one(right, id, typearg);
    }

    default: {}
  }

  assert(0);
  return false;
}

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs)
{
  assert(
    (ast_id(typeparams) == TK_TYPEPARAMS) ||
    (ast_id(typeparams) == TK_NONE)
    );
  assert(
    (ast_id(typeargs) == TK_TYPEARGS) ||
    (ast_id(typeargs) == TK_NONE)
    );

  if(ast_id(typeparams) == TK_NONE)
  {
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs, "type arguments where none were needed");
      return NULL;
    }

    return ast;
  }

  if(ast_id(typeargs) == TK_NONE)
  {
    ast_error(ast_parent(typeargs), "no type arguments where some were needed");
    return NULL;
  }

  // duplicate the node
  ast_t* r_ast = ast_dup(ast);

  // iterate pairwise through the params and the args
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    ast_t* id = ast_child(typeparam);

    if(!reify_one(r_ast, id, typearg))
      break;

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  if(typeparam != NULL)
  {
    ast_error(typeargs, "not enough type arguments");
    ast_free(r_ast);
    return NULL;
  }

  if(typearg != NULL)
  {
    ast_error(typearg, "too many type arguments");
    ast_free(r_ast);
    return NULL;
  }

  return r_ast;
}

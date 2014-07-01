#include "reify.h"
#include "nominal.h"
#include <assert.h>

static bool reify_nominal(ast_t* ast, ast_t* typeparam, ast_t* typearg)
{
  assert(ast_id(ast) == TK_NOMINAL);
  ast_t* param_name = ast_child(typeparam);
  ast_t* package = ast_child(ast);
  ast_t* name = ast_sibling(package);

  if(ast_id(package) != TK_NONE)
    return false;

  if(ast_name(param_name) != ast_name(name))
    return false;

  // TODO: keep the cap and ephemerality
  // swap in place
  ast_swap(ast, typearg);
  return true;
}

static bool reify_one(ast_t* ast, ast_t* typeparam, ast_t* typearg)
{
  ast_t* type = ast_type(ast);

  if((type != NULL) && !reify_one(type, typeparam, typearg))
    return false;

  if((ast_id(ast) == TK_NOMINAL) && reify_nominal(ast, typeparam, typearg))
    return true;

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    if(!reify_one(child, typeparam, typearg))
      return false;

    child = ast_sibling(child);
  }

  return true;
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
    // make sure typearg has nominal_def calculated in the right scope
    ast_t* sub_typearg = ast_child(typearg);

    if(ast_id(sub_typearg) == TK_NOMINAL)
      nominal_def(typearg, sub_typearg);

    // reify the typeparam with the typearg
    if(!reify_one(r_ast, typeparam, typearg))
      break;

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  if(typeparam != NULL)
  {
    // TODO: pick up default type arguments
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

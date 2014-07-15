#include "reify.h"
#include "nominal.h"
#include "subtype.h"
#include "../ast/token.h"
#include <assert.h>

static bool is_typeparam(ast_t* scope, ast_t* typeparam, ast_t* typearg)
{
  if(ast_id(typearg) != TK_NOMINAL)
    return false;

  ast_t* def = nominal_def(scope, typearg);
  return def == typeparam;
}

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

  // keep the cap and ephemerality
  nominal_applycap(ast, &typearg);

  // swap in place
  ast_replace(&ast, typearg);

  return true;
}

static void reify_one(ast_t* ast, ast_t* typeparam, ast_t* typearg)
{
  ast_t* type = ast_type(ast);

  if(type != NULL)
    reify_one(type, typeparam, typearg);

  if((ast_id(ast) == TK_NOMINAL) && reify_nominal(ast, typeparam, typearg))
    return;

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // read the next child first, since the current child might get replaced
    ast_t* next = ast_sibling(child);
    reify_one(child, typeparam, typearg);
    child = next;
  }
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

  // duplicate the node
  ast_t* r_ast = ast_dup(ast);

  // iterate pairwise through the params and the args
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    // reify the typeparam with the typearg
    reify_one(r_ast, typeparam, typearg);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  if(typearg != NULL)
  {
    ast_error(typearg, "too many type arguments");
    ast_free(r_ast);
    return NULL;
  }

  // pick up default type arguments if they exist
  while(typeparam != NULL)
  {
    typearg = ast_childidx(typeparam, 2);

    if(ast_id(typearg) == TK_NONE)
    {
      ast_error(typeargs, "not enough type arguments");
      ast_free(r_ast);
      return NULL;
    }

    reify_one(r_ast, typeparam, typearg);
    typeparam = ast_sibling(typeparam);
  }

  return r_ast;
}

bool check_constraints(ast_t* scope, ast_t* typeparams, ast_t* typeargs)
{
  // reify the type parameters with the typeargs
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
    if(!is_typeparam(scope, typeparam, typearg) &&
      !is_subtype(scope, typearg, constraint)
      )
    {
      ast_error(typearg, "type argument is outside its constraint");
      ast_error(typeparam, "constraint is here");
      ast_free_unattached(r_typeparams);
      return false;
    }

    r_typeparam = ast_sibling(r_typeparam);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  ast_free_unattached(r_typeparams);
  return true;
}

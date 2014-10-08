#include "reify.h"
#include "subtype.h"
#include "../ast/token.h"
#include "viewpoint.h"
#include <assert.h>

static bool reify_typeparamref(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_TYPEPARAMREF);
  ast_t* ref_name = ast_child(ast);
  ast_t* param_name = ast_child(typeparam);

  if(ast_name(ref_name) != ast_name(param_name))
    return false;

  typearg = ast_dup(typearg);
  reify_cap_and_ephemeral(ast, &typearg);

  ast_replace(astp, typearg);
  return true;
}

static bool reify_one(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  ast_t* type = ast_type(ast);

  if(type != NULL)
    reify_one(&type, typeparam, typearg);

  switch(ast_id(ast))
  {
    case TK_TYPEPARAMREF:
      if(reify_typeparamref(astp, typeparam, typearg))
        return true;
      break;

    case TK_ARROW:
    {
      // reify both sides
      ast_t* left = ast_child(ast);
      ast_t* right = ast_sibling(left);

      bool flatten = reify_one(&left, typeparam, typearg);
      flatten |= reify_one(&right, typeparam, typearg);

      // if we reified either side, flatten this arrow type to the viewpoint
      // adapted right side.
      if(flatten)
      {
        ast = viewpoint_type(left, right);
        ast_replace(astp, ast);
      }

      return flatten;
    }

    default: {}
  }

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    reify_one(&child, typeparam, typearg);
    child = ast_sibling(child);
  }

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

  // duplicate the node
  ast_t* r_ast = ast_dup(ast);

  // iterate pairwise through the params and the args
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    // reify the typeparam with the typearg
    reify_one(&r_ast, typeparam, typearg);
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

    reify_one(&r_ast, typeparam, typearg);
    typeparam = ast_sibling(typeparam);
  }

  return r_ast;
}

void reify_cap_and_ephemeral(ast_t* source, ast_t** target)
{
  int index;

  switch(ast_id(source))
  {
    case TK_NOMINAL: index = 3; break;
    case TK_STRUCTURAL: index = 1; break;
    case TK_TYPEPARAMREF: index = 1; break;
    default: assert(0); return;
  }

  ast_t* cap = ast_childidx(source, index);
  ast_t* ephemeral = ast_sibling(cap);

  if((ast_id(cap) == TK_NONE) && (ast_id(ephemeral) == TK_NONE))
    return;

  switch(ast_id(*target))
  {
    case TK_NOMINAL: index = 3; break;
    case TK_STRUCTURAL: index = 1; break;
    case TK_TYPEPARAMREF: index = 1; break;
    default: return;
  }

  ast_t* ast = ast_dup(*target);
  ast_t* tcap = ast_childidx(ast, index);
  ast_t* tephemeral = ast_sibling(tcap);

  if(ast_id(cap) != TK_NONE)
    ast_replace(&tcap, cap);

  if(ast_id(ephemeral) != TK_NONE)
    ast_replace(&tephemeral, ephemeral);

  *target = ast;
}

bool check_constraints(ast_t* typeparams, ast_t* typeargs)
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

    // TODO: is iso/trn a subtype of a ref/val/box for constraints? no.
    if(!is_subtype(typearg, constraint))
    {
      // TODO: remove this
      is_subtype(typearg, constraint);

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

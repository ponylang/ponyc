#include "flatten.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/cap.h"
#include <assert.h>

ast_result_t flatten_typeparamref(ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, cap, ephemeral);

  // Get the lowest capability that could fulfill the constraint.
  ast_t* def = (ast_t*)ast_data(ast);
  AST_GET_CHILDREN(def, name, constraint, default_type);

  if(ast_id(cap) != TK_NONE)
  {
    ast_error(cap, "can't specify a capability on a type parameter");
    return AST_ERROR;
  }

  // Set the typeparamref cap.
  token_id tcap = cap_from_constraint(constraint);
  ast_setid(cap, tcap);

  return AST_OK;
}

static ast_result_t flatten_noconstraint(typecheck_t* t, ast_t* ast)
{
  if(t->frame->constraint != NULL)
  {
    switch(ast_id(ast))
    {
      case TK_TUPLETYPE:
        ast_error(ast, "tuple types can't be used as constraints");
        return AST_ERROR;

      case TK_ARROW:
        if(t->frame->method == NULL)
        {
          ast_error(ast, "arrow types can't be used as type constraints");
          return AST_ERROR;
        }
        break;

      default: {}
    }
  }

  return AST_OK;
}

static ast_result_t flatten_sendable_params(ast_t* params)
{
  ast_t* param = ast_child(params);
  ast_result_t r = AST_OK;

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, type, def);

    if(!sendable(type))
    {
      ast_error(type, "this parameter must be sendable (iso, val or tag)");
      r = AST_ERROR;
    }

    param = ast_sibling(param);
  }

  return r;
}

static ast_result_t flatten_constructor(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_VAL:
      return flatten_sendable_params(params);

    default: {}
  }

  return AST_OK;
}

static ast_result_t flatten_async(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  return flatten_sendable_params(params);
}

ast_result_t pass_flatten(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_NEW:
    {
      switch(ast_id(t->frame->type))
      {
        case TK_CLASS:
          return flatten_constructor(ast);

        case TK_ACTOR:
          return flatten_async(ast);

        default: {}
      }
      break;
    }

    case TK_BE:
      return flatten_async(ast);

    case TK_UNIONTYPE:
      if(!flatten_union(astp))
        return AST_ERROR;
      break;

    case TK_ISECTTYPE:
      if(!flatten_isect(astp))
        return AST_ERROR;
      break;

    case TK_TUPLETYPE:
    case TK_ARROW:
      return flatten_noconstraint(t, ast);

    case TK_TYPEPARAMREF:
      return flatten_typeparamref(ast);

    default: {}
  }

  return AST_OK;
}

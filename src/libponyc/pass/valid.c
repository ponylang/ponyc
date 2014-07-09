#include "valid.h"
#include "../type/nominal.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool valid_ephemeral(ast_t* ast)
{
  if((ast_id(ast) == TK_HAT) && (ast_enclosing_method_type(ast) == NULL))
  {
    ast_error(ast,
      "ephemeral types can only appear in function return types");
    return false;
  }

  return true;
}

static bool valid_nominal(ast_t* ast)
{
  if(!nominal_valid(ast, &ast))
    return false;

  if(ast_id(ast) != TK_NOMINAL)
    return true;

  return valid_ephemeral(ast_childidx(ast, 4));
}

static bool valid_structural(ast_t* ast)
{
  return valid_ephemeral(ast_childidx(ast, 2));
}

static bool valid_thistype(ast_t* ast)
{
  ast_t* parent = ast_parent(ast);

  if(ast_id(parent) != TK_ARROW)
  {
    ast_error(ast, "in a type, 'this' can only be used as a viewpoint");
    return false;
  }

  if((ast_id(ast_parent(parent)) == TK_ARROW) || (ast_child(parent) != ast))
  {
    ast_error(ast, "when using 'this' for viewpoint it must come first");
    return false;
  }

  if(ast_enclosing_method(ast) == NULL)
  {
    ast_error(ast, "can only use 'this' for a viewpoint in a method");
    return false;
  }

  return true;
}

static bool valid_arrow(ast_t* ast)
{
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_THISTYPE:
      return true;

    case TK_NOMINAL:
    {
      ast_t* def = nominal_def(ast, left);
      assert(def != NULL);

      if(ast_id(def) == TK_TYPEPARAM)
      {
        // TODO: too late! we've already filled in a capability here
        // ast_t* cap = ast_childidx(left, 3);
        //
        // if(ast_id(cap) != TK_NONE)
        // {
        //   ast_error(cap, "can't specify a capability in a viewpoint");
        //   return false;
        // }

        ast_t* ephemeral = ast_childidx(left, 4);

        if(ast_id(ephemeral) != TK_NONE)
        {
          ast_error(ephemeral, "can't use an ephemeral type in a viewpoint");
          return false;
        }

        return true;
      }

      break;
    }

    default: {}
  }

  ast_error(left, "only type parameters and 'this' can be viewpoints");
  return false;
}

ast_result_t pass_valid(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!valid_nominal(ast))
        return AST_ERROR;
      break;

    case TK_STRUCTURAL:
      if(!valid_structural(ast))
        return AST_ERROR;
      break;

    case TK_THISTYPE:
      if(!valid_thistype(ast))
        return AST_ERROR;
      break;

    case TK_ARROW:
      if(!valid_arrow(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}

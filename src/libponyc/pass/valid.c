#include "valid.h"
#include "../type/assemble.h"
#include "../type/nominal.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool valid_new(ast_t* ast)
{
  // return type is This ref^
  ast_t* result = ast_childidx(ast, 4);
  ast_t* type = type_for_this(ast, TK_REF, true);
  ast_replace(result, type);
  return true;
}

static bool valid_be(ast_t* ast)
{
  // return type is This tag
  ast_t* result = ast_childidx(ast, 4);
  ast_t* type = type_for_this(ast, TK_TAG, false);
  ast_replace(result, type);
  return true;
}

static bool valid_fun(ast_t* ast)
{
  ast_t* result = ast_childidx(ast, 4);

  if(ast_id(result) != TK_NONE)
    return true;

  // set the return type to None
  ast_t* type = nominal_builtin(ast, "None");
  ast_replace(result, type);

  // add None at the end of the body, if there is one
  ast_t* body = ast_childidx(ast, 6);

  if(ast_id(body) == TK_SEQ)
  {
    ast_t* last = ast_childlast(body);
    ast_t* ref = ast_from(last, TK_REFERENCE);
    ast_t* none = ast_from_string(last, stringtab("None"));
    ast_add(ref, none);
    ast_append(body, ref);
  }

  return true;
}

static bool valid_nominal(ast_t* ast)
{
  if(!nominal_valid(ast, ast))
    return false;

  ast_t* ephemeral = ast_childidx(ast, 4);

  if((ast_id(ephemeral) == TK_HAT) && (ast_enclosing_method_type(ast) == NULL))
  {
    ast_error(ephemeral,
      "ephemeral types can only appear in function return types");
    return false;
  }

  return true;
}

static bool valid_structural(ast_t* ast)
{
  ast_t* cap = ast_childidx(ast, 1);
  ast_t* ephemeral = ast_sibling(cap);

  if((ast_id(ephemeral) == TK_HAT) && (ast_enclosing_method_type(ast) == NULL))
  {
    ast_error(ephemeral,
      "ephemeral types can only appear in function return types");
    return false;
  }

  if(ast_id(cap) != TK_NONE)
    return true;

  token_id def_cap;

  // if it's a typeparam, default capability is tag, otherwise it is ref
  if(ast_nearest(ast, TK_TYPEPARAM) != NULL)
    def_cap = TK_TAG;
  else
    def_cap = TK_REF;

  ast_replace(cap, ast_from(ast, def_cap));
  return true;
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
      // left side will already have been validated
      ast_t* def = nominal_def(ast, left);

      if(ast_id(def) == TK_TYPEPARAM)
        return true;

      break;
    }

    default: {}
  }

  ast_error(left, "only type parameters and 'this' can be viewpoints");
  return false;
}

ast_result_t pass_valid(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_NEW:
      if(!valid_new(ast))
        return AST_ERROR;
      break;

    case TK_BE:
      if(!valid_be(ast))
        return AST_ERROR;
      break;

    case TK_FUN:
      if(!valid_fun(ast))
        return AST_ERROR;
      break;

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

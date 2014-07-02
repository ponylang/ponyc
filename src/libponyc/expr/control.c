#include "control.h"
#include "../type/nominal.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include <assert.h>

bool expr_seq(ast_t* ast)
{
  // if any element can error, the whole thing can error
  ast_t* child = ast_child(ast);
  assert(child != NULL);

  ast_t* error = ast_from(ast, TK_ERROR);
  bool can_error = false;
  ast_t* type;

  while(child != NULL)
  {
    type = ast_type(child);
    can_error |= is_subtype(ast, error, type);
    child = ast_sibling(child);
  }

  if(can_error)
    type = type_union(ast, type, error);

  ast_settype(ast, type);
  ast_free_unattached(error);

  return true;
}

bool expr_if(ast_t* ast)
{
  ast_t* cond = ast_child(ast);
  ast_t* left = ast_sibling(cond);
  ast_t* right = ast_sibling(left);

  if(type_bool(cond) == NULL)
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_t* l_type = ast_type(left);
  ast_t* r_type;

  if(ast_id(right) == TK_NONE)
    r_type = nominal_builtin(ast, "None");
  else
    r_type = ast_type(right);

  ast_t* type = type_union(ast, l_type, r_type);
  ast_settype(ast, type);
  return true;
}

bool expr_while(ast_t* ast)
{
  ast_t* cond = ast_child(ast);

  if(type_bool(cond) == NULL)
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_settype(ast, nominal_builtin(ast, "None"));
  return true;
}

bool expr_repeat(ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* cond = ast_sibling(body);

  if(type_bool(cond) == NULL)
  {
    ast_error(cond, "condition must be a Bool");
    return false;
  }

  ast_settype(ast, nominal_builtin(ast, "None"));
  return true;
}

bool expr_try(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  // it has to be possible for the left side to result in an error
  ast_t* l_type = ast_type(left);
  ast_t* error = ast_from(ast, TK_ERROR);
  bool ok = is_subtype(ast, error, l_type);
  ast_free(error);

  if(!ok)
  {
    ast_error(left, "try expression never results in an error");
    return false;
  }

  ast_t* r_type;

  if(ast_id(right) == TK_NONE)
    r_type = nominal_builtin(ast, "None");
  else
    r_type = ast_type(right);

  switch(ast_id(l_type))
  {
    case TK_ERROR:
    {
      ast_settype(ast, r_type);
      return true;
    }

    case TK_UNIONTYPE:
    {
      // strip error out of the l_type
      ast_t* type = type_union(ast, type_strip_error(ast, l_type), r_type);
      ast_settype(ast, type);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool expr_continue(ast_t* ast)
{
  ast_t* loop = ast_enclosing_loop(ast);

  if(loop == NULL)
  {
    ast_error(ast, "must be in a loop");
    return false;
  }

  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "is followed with this expression");
    return false;
  }

  ast_settype(ast, nominal_builtin(ast, "None"));
  return true;
}

bool expr_return(ast_t* ast)
{
  ast_t* body = ast_child(ast);
  ast_t* type = ast_type(body);
  ast_t* fun = ast_enclosing_method_body(ast);
  bool ok = true;

  if(fun == NULL)
  {
    ast_error(ast, "return must occur in a function or a behaviour body");
    return false;
  }

  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "is followed with this expression");
    ok = false;
  }

  switch(ast_id(fun))
  {
    case TK_NEW:
      ast_error(ast, "cannot return in a constructor");
      return false;

    case TK_BE:
    {
      ast_t* none = nominal_builtin(ast, "None");

      if(!is_subtype(ast, type, none))
      {
        ast_error(body, "body of a return in a behaviour must have type None");
        ok = false;
      }

      ast_free(none);
      return ok;
    }

    case TK_FUN:
    {
      ast_t* result = ast_childidx(fun, 4);

      if(ast_id(result) == TK_NONE)
        result = nominal_builtin(ast, "None");

      if(!is_subtype(ast, type, result))
      {
        ast_error(body,
          "body of return doesn't match the function return type");
        ok = false;
      }

      ast_free_unattached(result);
      return ok;
    }

    default: {}
  }

  assert(0);
  return false;
}

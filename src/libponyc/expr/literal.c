#include "literal.h"
#include "../ast/token.h"
#include "../pass/names.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/reify.h"
#include "../type/cap.h"
#include <assert.h>

bool expr_literal(ast_t* ast, const char* name)
{
  ast_t* type = type_builtin(ast, name);

  if(type == NULL)
    return false;

  ast_settype(ast, type);
  return true;
}

bool expr_this(ast_t* ast)
{
  ast_t* type = type_for_this(ast, cap_for_receiver(ast), false);
  ast_settype(ast, type);

  ast_t* nominal;

  if(ast_id(type) == TK_NOMINAL)
    nominal = type;
  else
    nominal = ast_childidx(type, 1);

  ast_t* typeargs = ast_childidx(nominal, 2);
  ast_t* typearg = ast_child(typeargs);

  while(typearg != NULL)
  {
    if(!expr_nominal(&typearg))
    {
      ast_error(ast, "couldn't create a type for 'this'");
      ast_free(type);
      return false;
    }

    typearg = ast_sibling(typearg);
  }

  if(!expr_nominal(&nominal))
  {
    ast_error(ast, "couldn't create a type for 'this'");
    ast_free(type);
    return false;
  }

  return true;
}

bool expr_tuple(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type;

  if(ast_sibling(child) == NULL)
  {
    type = ast_type(child);
  } else {
    type = ast_type(child);
    child = ast_sibling(child);

    while(child != NULL)
    {
      ast_t* tuple = ast_from(ast, TK_TUPLETYPE);
      ast_add(tuple, ast_type(child));
      ast_add(tuple, type);
      type = tuple;

      child = ast_sibling(child);
    }
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_error(ast_t* ast)
{
  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "error must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "error is followed with this expression");
    return false;
  }

  ast_seterror(ast);
  return true;
}

bool expr_compiler_intrinsic(ast_t* ast)
{
  ast_t* fun = ast_enclosing_method_body(ast);
  ast_t* body = ast_childidx(fun, 6);
  ast_t* child = ast_child(body);

  if((child != ast) || (ast_sibling(child) != NULL))
  {
    ast_error(ast, "a compiler intrinsic must be the entire body");
    return false;
  }

  ast_settype(ast, ast_from(ast, TK_COMPILER_INTRINSIC));
  return true;
}

bool expr_nominal(ast_t** astp)
{
  // resolve typealias/typeparamref
  if(!names_nominal(*astp, astp))
    return false;

  ast_t* ast = *astp;

  if(ast_id(ast) != TK_NOMINAL)
    return true;

  // if still nominal, check constraints
  ast_t* def = ast_data(ast);
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* typeargs = ast_childidx(ast, 2);

  return check_constraints(typeparams, typeargs);
}

bool expr_fun(ast_t* ast)
{
  ast_t* type = ast_childidx(ast, 4);
  ast_t* can_error = ast_sibling(type);
  ast_t* body = ast_sibling(can_error);

  if(ast_id(body) == TK_NONE)
    return true;

  ast_t* def = ast_enclosing_type(ast);
  bool is_trait = ast_id(def) == TK_TRAIT;

  // if specified, body type must match return type
  ast_t* body_type = ast_type(body);

  if(body_type == NULL)
  {
    ast_t* last = ast_childlast(body);
    ast_error(type, "function body always results in an error");
    ast_error(last, "function body expression is here");
    return false;
  }

  if(ast_id(body_type) == TK_COMPILER_INTRINSIC)
    return true;

  // check partial functions
  if(ast_id(can_error) == TK_QUESTION)
  {
    // if a partial function, check that we might actually error
    if(!is_trait && !ast_canerror(body))
    {
      ast_error(can_error, "function body is not partial but the function is");
      return false;
    }
  } else {
    // if not a partial function, check that we can't error
    if(ast_canerror(body))
    {
      ast_error(can_error, "function body is partial but the function is not");
      return false;
    }
  }

  if(ast_id(ast) == TK_FUN)
  {
    if(!is_subtype(body_type, type))
    {
      ast_t* last = ast_childlast(body);
      ast_error(type, "function body isn't a subtype of the result type");
      ast_error(last, "function body expression is here");
      return false;
    }

    if(!is_trait && !is_eqtype(body_type, type))
    {
      // it's ok to return a literal where an arithmetic type is expected
      if(is_builtin(body_type, "IntLiteral"))
      {
        ast_t* math = type_builtin(ast, "Arithmetic");
        bool ok = is_subtype(type, math);
        ast_free(math);

        if(ok)
          return true;
      } else if(is_builtin(body_type, "FloatLiteral")) {
        ast_t* math = type_builtin(ast, "Float");
        bool ok = is_subtype(type, math);
        ast_free(math);

        if(ok)
          return true;
      }

      ast_t* last = ast_childlast(body);
      ast_error(type, "function body is more specific than the result type");
      ast_error(last, "function body expression is here");
      return false;
    }
  }

  return true;
}

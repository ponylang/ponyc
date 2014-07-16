#include "operator.h"
#include "literal.h"
#include "postfix.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/alias.h"
#include "../type/viewpoint.h"
#include <assert.h>

static bool is_lvalue(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    case TK_FVARREF:
    case TK_FLETREF: // TODO: only valid the first time in a constructor
    case TK_VARREF:
    case TK_PARAMREF:
      return true;

    case TK_TUPLE:
    {
      // a tuple is an lvalue if every component expression is an lvalue
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!is_lvalue(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    default: {}
  }

  return false;
}

static bool binop_to_function(ast_t** astp)
{
  ast_t* ast = *astp;
  const char* method;

  switch(ast_id(ast))
  {
    case TK_EQ: method = "eq"; break;
    case TK_NE: method = "ne"; break;
    case TK_LT: method = "lt"; break;
    case TK_LE: method = "le"; break;
    case TK_GE: method = "ge"; break;
    case TK_GT: method = "gt"; break;

    default:
      assert(0);
      return false;
  }

  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  // look up the method on the left side
  ast_t* dot = ast_from(ast, TK_DOT);
  ast_add(dot, ast_from_string(ast, method));
  ast_add(dot, left);

  // call the method with the right side
  ast_t* positional = ast_from(ast, TK_POSITIONALARGS);
  ast_add(positional, right);

  ast_t* call = ast_from(ast, TK_CALL);
  ast_add(call, ast_from(ast, TK_NONE)); // named args
  ast_add(call, positional); // positional args
  ast_add(call, dot);

  // replace with the function call
  ast_replace(astp, call);

  if(!expr_dot(dot))
    return false;

  return expr_call(call);
}

bool expr_identity(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!is_id_compatible(l_type, r_type))
  {
    ast_error(ast, "left and right side must have related types");
    return false;
  }

  ast_settype(ast, type_builtin(ast, "Bool"));
  ast_inheriterror(ast);
  return true;
}

bool expr_compare(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!is_arithmetic(l_type))
    return binop_to_function(astp);

  if(!is_math_compatible(l_type, r_type))
  {
    ast_error(ast, "arithmetic comparison must be on the same type");
    return false;
  }

  ast_settype(ast, type_builtin(ast, "Bool"));
  ast_inheriterror(ast);
  return true;
}

bool expr_order(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!is_arithmetic(l_type))
    return binop_to_function(astp);

  if(!is_math_compatible(l_type, r_type))
  {
    ast_error(ast, "arithmetic ordering must be on the same type");
    return false;
  }

  ast_settype(ast, type_builtin(ast, "Bool"));
  ast_inheriterror(ast);
  return true;
}

bool expr_arithmetic(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!is_arithmetic(l_type) || !is_math_compatible(l_type, r_type))
  {
    ast_error(ast, "left and right side must be the same arithmetic type");
    return false;
  }

  ast_settype(ast, l_type);
  ast_inheriterror(ast);
  return true;
}

bool expr_minus(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = ast_type(child);

  if(!is_arithmetic(type))
  {
    ast_error(ast, "unary minus is only allowed on arithmetic types");
    return false;
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_shift(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!is_integer(l_type) || !is_integer(r_type))
  {
    ast_error(ast, "shift is only allowed on integer types");
    return false;
  }

  ast_settype(ast, l_type);
  ast_inheriterror(ast);
  return true;
}

bool expr_logical(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(is_bool(l_type) && is_bool(r_type))
  {
    ast_settype(ast, type_builtin(ast, "Bool"));
  } else if(is_integer(l_type) && is_integer(r_type)) {
    ast_settype(ast, l_type);
  } else {
    ast_error(ast,
      "left and right side must be of boolean or integer type");
    return false;
  }

  ast_inheriterror(ast);
  return true;
}

bool expr_not(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = ast_type(child);

  if(is_bool(type))
  {
    ast_settype(ast, type_builtin(ast, "Bool"));
  } else if(is_arithmetic(type)) {
    ast_settype(ast, type);
  } else {
    ast_error(ast, "not is only allowed on boolean or arithmetic types");
    return false;
  }

  ast_inheriterror(ast);
  return true;
}

bool expr_assign(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!is_lvalue(left))
  {
    ast_error(ast, "left side must be something that can be assigned to");
    return false;
  }

  // assignment is based on the alias of the right hand side
  ast_t* a_type = alias(r_type);

  if(l_type == NULL)
  {
    // local type inference
    assert((ast_id(left) == TK_VAR) || (ast_id(left) == TK_LET));

    // returns the right side since there was no previous value to read
    ast_settype(ast, a_type);

    // set the type for each component
    return type_for_idseq(ast_child(left), a_type);
  }

  bool ok_sub = is_subtype(a_type, l_type);
  bool ok_safe = safe_to_write(left, a_type);
  ast_free_unattached(a_type);

  if(!ok_sub)
  {
    ast_error(ast, "right side must be a subtype of left side");
    return false;
  }

  if(!ok_safe)
  {
    ast_error(ast, "not safe to write right side to left side");
    return false;
  }

  ast_settype(ast, consume_type(l_type));
  ast_inheriterror(ast);
  return true;
}

bool expr_consume(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = ast_type(child);

  // TODO: handle removing consumed identifiers from the scope
  ast_settype(ast, consume_type(type));
  return true;
}

bool expr_recover(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = ast_type(child);

  // TODO: handle removing unsendable things from the child scope
  ast_settype(ast, recover_type(type));
  return true;
}

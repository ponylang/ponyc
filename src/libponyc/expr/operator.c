#include "operator.h"
#include "literal.h"
#include "postfix.h"
#include "../type/nominal.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/alias.h"
#include "../ds/stringtab.h"
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
    case TK_EQ: method = stringtab("eq"); break;
    case TK_NE: method = stringtab("ne"); break;
    case TK_LT: method = stringtab("lt"); break;
    case TK_LE: method = stringtab("le"); break;
    case TK_GE: method = stringtab("ge"); break;
    case TK_GT: method = stringtab("gt"); break;

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

  if(type_super(ast, l_type, r_type) == NULL)
  {
    // TODO: allow this for unrelated structural types?
    ast_error(ast, "left and right side must have related types");
    return false;
  }

  ast_settype(ast, nominal_builtin(ast, "Bool"));
  ast_inheriterror(ast);
  return true;
}

bool expr_compare(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_arithmetic(left);
  ast_t* r_type = type_arithmetic(right);
  ast_t* type = type_super(ast, l_type, r_type);

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);

  if(type == NULL)
    return binop_to_function(astp);

  ast_settype(ast, nominal_builtin(ast, "Bool"));
  ast_inheriterror(ast);
  return true;
}

bool expr_order(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_arithmetic(left);
  ast_t* r_type = type_arithmetic(right);
  ast_t* type = type_super(ast, l_type, r_type);

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);

  if(type == NULL)
    return binop_to_function(astp);

  ast_settype(ast, nominal_builtin(ast, "Bool"));
  ast_inheriterror(ast);
  return true;
}

bool expr_arithmetic(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_arithmetic(left);
  ast_t* r_type = type_arithmetic(right);
  ast_t* type = type_super(ast, l_type, r_type);

  if(type == NULL)
    ast_error(ast, "left and right side must have related arithmetic types");
  else
    ast_settype(ast, type);

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);
  ast_inheriterror(ast);

  return (type != NULL);
}

bool expr_minus(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = type_arithmetic(left);
  ast_t* r_type = NULL;
  ast_t* type;

  if(right != NULL)
  {
    r_type = type_arithmetic(right);
    type = type_super(ast, l_type, r_type);

    if(type == NULL)
      ast_error(ast, "left and right side must have related arithmetic types");
  } else {
    type = l_type;

    if(type == NULL)
      ast_error(ast, "must have an arithmetic type");
  }

  if(type != NULL)
    ast_settype(ast, type);

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);
  ast_inheriterror(ast);

  return (type != NULL);
}

bool expr_shift(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_int(left);
  ast_t* r_type = type_int(right);

  if((l_type == NULL) || (r_type == NULL))
  {
    ast_error(ast,
      "left and right side must have integer types");
  } else {
    ast_settype(ast, l_type);
  }

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);
  ast_inheriterror(ast);

  return (l_type != NULL) && (r_type != NULL);
}

bool expr_logical(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_int_or_bool(left);
  ast_t* r_type = type_int_or_bool(right);
  ast_t* type = type_super(ast, l_type, r_type);

  if(type == NULL)
  {
    ast_error(ast,
      "left and right side must have related integer or boolean types");
  } else {
    ast_settype(ast, type);
  }

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);
  ast_inheriterror(ast);

  return (type != NULL);
}

bool expr_not(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = type_int_or_bool(child);

  if(type == NULL)
    return false;

  ast_settype(ast, type);
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

  bool ok = is_subtype(ast, a_type, l_type);
  ast_free_unattached(a_type);

  if(!ok)
  {
    ast_error(ast, "right side must be a subtype of left side");
    return false;
  }

  // TODO: viewpoint adaptation, safe to write
  ast_settype(ast, l_type);
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

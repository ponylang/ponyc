#include "operator.h"
#include "literal.h"
#include "postfix.h"
#include "control.h"
#include "reference.h"
#include "../ast/token.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/alias.h"
#include "../type/viewpoint.h"
#include "../pass/expr.h"
#include <assert.h>

static bool assign_id(ast_t* ast, bool let)
{
  assert(ast_id(ast) == TK_ID);
  const char* name = ast_name(ast);

  sym_status_t status;
  ast_get(ast, name, &status);

  switch(status)
  {
    case SYM_UNDEFINED:
      ast_setstatus(ast, name, SYM_DEFINED);
      return true;

    case SYM_DEFINED:
      if(let)
      {
        ast_error(ast, "can't assign to a let definition more than once");
        return false;
      }

      return true;

    case SYM_CONSUMED:
      ast_error(ast, "can't assign to a consumed local");
      return false;

    default: {}
  }

  assert(0);
  return false;
}

static bool is_lvalue(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    {
      ast_t* idseq = ast_child(ast);
      ast_t* id = ast_child(idseq);

      while(id != NULL)
      {
        if(!assign_id(id, ast_id(ast) == TK_LET))
          return false;

        id = ast_sibling(id);
      }

      return true;
    }

    case TK_VARREF:
    {
      ast_t* id = ast_child(ast);
      return assign_id(id, false);
    }

    case TK_LETREF:
    {
      ast_error(ast, "can't assign to a let local");
      return false;
    }

    case TK_FVARREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) == TK_THIS)
        return assign_id(right, false);

      return true;
    }

    case TK_FLETREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
      {
        ast_error(ast, "can't assign to a let field");
        return false;
      }

      if(ast_enclosing_loop(ast) != NULL)
      {
        ast_error(ast, "can't assign to a let field in a loop");
        return false;
      }

      return assign_id(right, true);
    }

    case TK_TUPLE:
    {
      // A tuple is an lvalue if every component expression is an lvalue.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!is_lvalue(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_SEQ:
    {
      // A sequence is an lvalue if it has a single child that is an lvalue.
      // This is used because the components of a tuple are sequences.
      ast_t* child = ast_child(ast);

      if(ast_sibling(child) != NULL)
        return false;

      return is_lvalue(child);
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

static ast_t* make_tuple_index(ast_t* tuple, int index)
{
  BUILD(dot, tuple, NODE(TK_DOT, TREE(tuple) INT(index)));

  if(!expr_dot(dot))
  {
    ast_free_unattached(dot);
    return NULL;
  }

  return dot;
}

static ast_t* make_binop(token_id op, ast_t* left, ast_t* right)
{
  if(left == NULL)
    return right;

  if(right == NULL)
  {
    ast_free_unattached(left);
    return NULL;
  }

  BUILD(binop, left, NODE(op, TREE(left) TREE(right)));

  if(pass_expr(&binop) != AST_OK)
  {
    ast_free_unattached(binop);
    return NULL;
  }

  return binop;
}

static bool tuples_pairwise(ast_t** astp, ast_t* left, ast_t* right)
{
  ast_t* ast = *astp;
  token_id op = ast_id(ast);
  token_id logic;

  switch(op)
  {
    case TK_IS:
    case TK_EQ:
      logic = TK_AND;
      break;

    case TK_ISNT:
    case TK_NE:
      logic = TK_OR;
      break;

    default:
      assert(0);
      return false;
  }

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);
  ast_t* l_child = ast_child(l_type);
  ast_t* r_child = ast_child(r_type);

  if(ast_id(l_type) != TK_TUPLETYPE)
  {
    ast_error(left, "must be a tuple");
    return false;
  }

  if(ast_id(r_type) != TK_TUPLETYPE)
  {
    ast_error(right, "must be a tuple");
    return false;
  }

  ast_t* node = NULL;
  int i = 0;

  while((l_child != NULL) && (r_child != NULL))
  {
    ast_t* l_dot = make_tuple_index(left, i);
    ast_t* r_dot = make_tuple_index(right, i);

    if((l_dot == NULL) || (r_dot == NULL))
    {
      ast_free_unattached(node);
      return false;
    }

    ast_t* binop = make_binop(op, l_dot, r_dot);
    node = make_binop(logic, node, binop);

    if(node == NULL)
      return false;

    l_child = ast_sibling(l_child);
    r_child = ast_sibling(r_child);
    i++;
  }

  if((l_child != NULL) || (r_child != NULL))
  {
    ast_error(ast, "tuples are of different lengths");
    ast_free_unattached(node);
    return false;
  }

  ast_replace(astp, node);
  return true;
}

bool expr_identity(ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  // Handle tuples as pairwise ordering.
  if(ast_id(l_type) == TK_TUPLETYPE)
    return tuples_pairwise(astp, left, right);

  if(is_math_compatible(l_type, r_type) ||
    (is_bool(l_type) && is_bool(r_type))
    )
  {
    switch(ast_id(ast))
    {
      case TK_IS:
        ast_setid(ast, TK_EQ);
        break;

      case TK_ISNT:
        ast_setid(ast, TK_NE);
        break;

      default:
        assert(0);
        return false;
    }
  } else if(!is_id_compatible(l_type, r_type)) {
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

  // Handle tuples as pairwise comparison.
  if(ast_id(l_type) == TK_TUPLETYPE)
    return tuples_pairwise(astp, left, right);

  // Use a.eq(b) if neither arithmetic nor boolean.
  if(!is_arithmetic(l_type) && !is_bool(l_type))
    return binop_to_function(astp);

  if(!is_math_compatible(l_type, r_type) &&
    (!is_bool(l_type) || !is_bool(r_type))
    )
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

  // Handle tuples as pairwise ordering.
  if(ast_id(l_type) == TK_TUPLETYPE)
  {
    // return tuples_pairwise(astp, left, right);
    ast_error(ast, "ordering is not defined for tuples");
    return false;
  }

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

  // TODO: string concatenation
  // TODO: treat some tuples as vectors?
  if(!is_arithmetic(l_type) || !is_math_compatible(l_type, r_type))
  {
    ast_error(ast, "left and right side must be the same arithmetic type");
    return false;
  }

  // pick the correct node type
  if((ast_id(ast) == TK_MINUS) &&
    is_intliteral(l_type) &&
    is_intliteral(r_type))
  {
    ast_settype(ast, type_builtin(ast, "SIntLiteral"));
  } else if(is_uintliteral(l_type)) {
    ast_settype(ast, r_type);
  } else if(is_sintliteral(l_type) && !is_uintliteral(r_type)) {
    ast_settype(ast, r_type);
  } else if(is_floatliteral(l_type) && !is_intliteral(r_type)) {
    ast_settype(ast, r_type);
  } else {
    ast_settype(ast, l_type);
  }

  ast_inheriterror(ast);
  return true;
}

bool expr_minus(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = ast_type(child);

  // TODO: treat some tuples as vectors?
  if(!is_arithmetic(type))
  {
    ast_error(ast, "unary minus is only allowed on arithmetic types");
    return false;
  }

  if(is_uintliteral(type))
    ast_settype(ast, type_builtin(type, "SIntLiteral"));
  else
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

  // TODO: treat some tuples as vectors?
  if(!is_integer(l_type) || !is_math_compatible(l_type, r_type))
  {
    ast_error(ast, "left and right side must be the same integer type");
    return false;
  }

  // pick the correct node type
  if(is_intliteral(l_type))
    ast_settype(ast, r_type);
  else
    ast_settype(ast, l_type);

  ast_inheriterror(ast);
  return true;
}

bool expr_and(ast_t** astp)
{
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, left, right);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(is_bool(l_type) && is_bool(r_type))
  {
    // Rewrite as: if left then right else False end
    REPLACE(astp,
      NODE(TK_IF,
        NODE(TK_SEQ, TREE(left))
        NODE(TK_SEQ, TREE(right))
        NODE(TK_SEQ, NODE(TK_REFERENCE, ID("False")))
        )
      );

    ast = *astp;
    AST_GET_CHILDREN(ast, cond, l_branch, r_branch);
    AST_GET_CHILDREN(r_branch, ref);

    return expr_reference(ref) && expr_seq(cond) && expr_seq(l_branch) &&
      expr_seq(r_branch) && expr_if(ast);
  }

  return expr_logical(ast);
}

bool expr_or(ast_t** astp)
{
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, left, right);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(is_bool(l_type) && is_bool(r_type))
  {
    // Rewrite as: if left then True else right end
    REPLACE(astp,
      NODE(TK_IF,
        NODE(TK_SEQ, TREE(left))
        NODE(TK_SEQ, NODE(TK_REFERENCE, ID("True")))
        NODE(TK_SEQ, TREE(right))
        )
      );

    ast = *astp;
    AST_GET_CHILDREN(ast, cond, l_branch, r_branch);
    AST_GET_CHILDREN(l_branch, ref);

    return expr_reference(ref) && expr_seq(cond) && expr_seq(l_branch) &&
      expr_seq(r_branch) && expr_if(ast);
  }

  return expr_logical(ast);
}

bool expr_logical(ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(is_bool(l_type) && is_bool(r_type))
  {
    ast_settype(ast, type_builtin(ast, "Bool"));
  } else if(is_integer(l_type) && is_integer(r_type)) {
    // TODO: treat some tuples as vectors?
    if(!is_math_compatible(l_type, r_type))
    {
      ast_error(ast,
        "left and right side must be the same arithmetic type");
      return false;
    }

    // pick the correct node type
    if(is_intliteral(l_type))
      ast_settype(ast, r_type);
    else
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
    // TODO: treat some tuples as vectors?
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
    a_type = type_literal_to_runtime(a_type);

    // returns the right side since there was no previous value to read
    ast_settype(ast, a_type);

    // set the type node
    AST_GET_CHILDREN(left, idseq, type);
    ast_replace(&type, a_type);
    ast_settype(left, a_type);

    // set the type for each component
    return type_for_idseq(idseq, a_type);
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

  switch(ast_id(child))
  {
    case TK_VARREF:
    case TK_LETREF:
    case TK_PARAMREF:
      break;

    default:
      ast_error(ast, "consume must take a local or parameter");
      return false;
  }

  ast_t* id = ast_child(child);
  const char* name = ast_name(id);

  // Can't consume from an outer scope while inside a loop.
  ast_t* loop = ast_enclosing_loop(ast);

  if((loop != NULL) && !ast_within_scope(loop, ast, name))
  {
    ast_error(ast, "can't consume from an outer scope in a loop");
    return false;
  }

  ast_setstatus(ast, name, SYM_CONSUMED);

  ast_t* type = ast_type(child);
  ast_settype(ast, consume_type(type));
  return true;
}

bool expr_recover(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = ast_type(child);

  ast_settype(ast, recover_type(type));
  return true;
}

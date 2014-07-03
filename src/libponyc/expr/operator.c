#include "operator.h"
#include "literal.h"
#include "../type/nominal.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/alias.h"
#include <assert.h>

static bool is_lvalue(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    case TK_FIELDREF:
    case TK_PARAMREF:
    case TK_LOCALREF:
      // an identifier reference is an lvalue. it may still not be valid to
      // assign to it (it could be a method or an SSA that's already set).
      // the same is true for accessing a member with dot notation.
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

  return expr_literal(ast, "Bool");
}

bool expr_compare(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_arithmetic(left);
  ast_t* r_type = type_arithmetic(right);
  ast_t* type = type_super(ast, l_type, r_type);

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);

  if(type == NULL)
  {
    l_type = ast_type(left);
    r_type = ast_type(right);

    if(!is_subtype(ast, r_type, l_type))
    {
      ast_error(ast, "right side must be a subtype of left side");
      return false;
    }

    // left side must be Comparable
    ast_t* comparable = nominal_builtin1(ast, "Comparable", r_type);
    bool ok = is_subtype(ast, l_type, comparable);
    ast_free(comparable);

    if(!ok)
    {
      ast_error(ast, "left side must be Comparable");
      return false;
    }

    // TODO: rewrite this as a function call?
  } else {
    switch(ast_id(ast))
    {
      case TK_EQ:
        ast_setid(ast, TK_IS);
        break;

      case TK_NE:
        ast_setid(ast, TK_ISNT);
        break;

      default:
        assert(0);
        return false;
    }
  }

  ast_settype(ast, nominal_builtin(ast, "Bool"));
  return true;
}

bool expr_order(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  ast_t* l_type = type_arithmetic(left);
  ast_t* r_type = type_arithmetic(right);
  ast_t* type = type_super(ast, l_type, r_type);

  ast_free_unattached(l_type);
  ast_free_unattached(r_type);

  if(type == NULL)
  {
    l_type = ast_type(left);
    r_type = ast_type(right);

    if(!is_subtype(ast, r_type, l_type))
    {
      ast_error(ast, "right side must be a subtype of left side");
      return false;
    }

    // left side must be Ordered
    ast_t* comparable = nominal_builtin1(ast, "Ordered", r_type);
    bool ok = is_subtype(ast, l_type, comparable);
    ast_free(comparable);

    if(!ok)
    {
      ast_error(ast, "left side must be Ordered");
      return false;
    }

    // TODO: rewrite this as a function call?
  }

  ast_settype(ast, nominal_builtin(ast, "Bool"));
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

  return (type != NULL);
}

bool expr_not(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type = type_int_or_bool(child);

  if(type == NULL)
    return false;

  ast_settype(ast, type);
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

  if(l_type == NULL)
  {
    assert((ast_id(left) == TK_VAR) || (ast_id(left) == TK_LET));

    // returns an alias since there was no previous value to read
    ast_settype(ast, alias(r_type));

    // set the type for each component
    return type_for_idseq(ast_child(left), r_type);
  } else if(!is_subtype(ast, r_type, l_type)) {
    ast_error(ast, "right side must be a subtype of left side");
    return false;
  }

  // TODO: viewpoint adaptation, safe to write, etc
  // TODO: disallow reassignment to SSA variable
  ast_settype(ast, l_type);
  return true;
}

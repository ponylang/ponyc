#include "operator.h"
#include "literal.h"
#include "postfix.h"
#include "control.h"
#include "reference.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/matchtype.h"
#include "../type/alias.h"
#include "../type/viewpoint.h"
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

bool expr_identity(ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);
  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(!could_subtype(l_type, r_type) && !could_subtype(r_type, l_type))
  {
    ast_error(ast, "left and right side must be related types");
    return false;
  }

  ast_settype(ast, type_builtin(ast, "Bool"));
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

    // set the type node
    AST_GET_CHILDREN(left, idseq, type);
    ast_replace(&type, a_type);

    ast_settype(left, a_type);
    ast_inheriterror(ast);

    // set the type for each component
    return type_for_idseq(idseq, a_type);
  }

  bool ok_sub = is_subtype(a_type, l_type);

  if(ok_sub)
    ok_sub = coerce_literals(right, l_type);

  if(!ok_sub)
  {
    ast_error(ast, "right side must be a subtype of left side");
    ast_free_unattached(a_type);
    return false;
  }

  bool ok_safe = safe_to_write(left, a_type);
  ast_free_unattached(a_type);

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

#include "operator.h"
#include "literal.h"
#include "postfix.h"
#include "control.h"
#include "reference.h"
#include "../ast/lexer.h"
#include "../pass/expr.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/matchtype.h"
#include "../type/safeto.h"
#include "../type/subtype.h"
#include <assert.h>

static bool assign_id(typecheck_t* t, ast_t* ast, bool let, bool need_value)
{
  assert(ast_id(ast) == TK_ID);
  const char* name = ast_name(ast);

  sym_status_t status;
  ast_get(ast, name, &status);

  switch(status)
  {
    case SYM_UNDEFINED:
      if(need_value)
      {
        ast_error(ast, "the left side is undefined but its value is used");
        return false;
      }

      ast_setstatus(ast, name, SYM_DEFINED);
      return true;

    case SYM_DEFINED:
      if(let)
      {
        ast_error(ast,
          "can't assign to a let or embed definition more than once");
        return false;
      }

      return true;

    case SYM_CONSUMED:
    {
      bool ok = true;

      if(need_value)
      {
        ast_error(ast, "the left side is consumed but its value is used");
        ok = false;
      }

      if(let)
      {
        ast_error(ast,
          "can't assign to a let or embed definition more than once");
        ok = false;
      }

      if(t->frame->try_expr != NULL)
      {
        ast_error(ast,
          "can't reassign to a consumed identifier in a try expression");
        ok = false;
      }

      if(ok)
        ast_setstatus(ast, name, SYM_DEFINED);

      return ok;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool is_lvalue(typecheck_t* t, ast_t* ast, bool need_value)
{
  switch(ast_id(ast))
  {
    case TK_DONTCARE:
      // Can only assign to it if we don't need the value.
      return !need_value;

    case TK_VAR:
    case TK_LET:
      return assign_id(t, ast_child(ast), ast_id(ast) == TK_LET, need_value);

    case TK_VARREF:
    {
      ast_t* id = ast_child(ast);
      return assign_id(t, id, false, need_value);
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
        return assign_id(t, right, false, need_value);

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

      if(t->frame->loop_body != NULL)
      {
        ast_error(ast, "can't assign to a let field in a loop");
        return false;
      }

      return assign_id(t, right, true, need_value);
    }

    case TK_EMBEDREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
      {
        ast_error(ast, "can't assign to an embed field");
        return false;
      }

      if(t->frame->loop_body != NULL)
      {
        ast_error(ast, "can't assign to an embed field in a loop");
        return false;
      }

      return assign_id(t, right, true, need_value);
    }

    case TK_TUPLE:
    {
      // A tuple is an lvalue if every component expression is an lvalue.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!is_lvalue(t, child, need_value))
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

      return is_lvalue(t, child, need_value);
    }

    default: {}
  }

  return false;
}

bool expr_identity(pass_opt_t* opt, ast_t* ast)
{
  ast_settype(ast, type_builtin(opt, ast, "Bool"));
  ast_inheritflags(ast);
  return true;
}


typedef struct infer_path_t
{
  size_t index;
  struct infer_path_t* next; // Next node
  struct infer_path_t* root; // Root node in list
} infer_path_t;

typedef enum infer_ret_t
{
  INFER_OK,
  INFER_NOP,
  INFER_ERROR
} infer_ret_t;

static ast_t* find_infer_type(ast_t* type, infer_path_t* path)
{
  assert(type != NULL);

  switch(ast_id(type))
  {
    case TK_TUPLETYPE:
      if(path == NULL)  // End of path, infer the whole tuple
        return type;

      if(path->index >= ast_childcount(type)) // Cardinality mismatch
        return NULL;

      return find_infer_type(ast_childidx(type, path->index), path->next);

    case TK_UNIONTYPE:
    {
      // Infer all children
      ast_t* u_type = NULL;

      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
      {
        ast_t* t = find_infer_type(p, path);

        if(t == NULL)
        {
          // Propogate error
          ast_free_unattached(u_type);
          return NULL;
        }

        u_type = type_union(u_type, t);
      }

      return u_type;
    }

    case TK_ISECTTYPE:
    {
      // Infer all children
      ast_t* i_type = NULL;

      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
      {
        ast_t* t = find_infer_type(p, path);

        if(t == NULL)
        {
          // Propogate error
          ast_free_unattached(i_type);
          return NULL;
        }

        i_type = type_isect(i_type, t);
      }

      return i_type;
    }

    default:
      if(path != NULL)  // Type doesn't match path
        return NULL;

      // Just return whatever this type is
      return type;
  }
}

static infer_ret_t infer_local_inner(ast_t* left, ast_t* r_type,
  infer_path_t* path)
{
  assert(left != NULL);
  assert(r_type != NULL);
  assert(path != NULL);
  assert(path->root != NULL);

  infer_ret_t ret_val = INFER_NOP;

  switch(ast_id(left))
  {
    case TK_SEQ:
    {
      assert(ast_childcount(left) == 1);
      infer_ret_t r = infer_local_inner(ast_child(left), r_type, path);

      if(r == INFER_OK) // Update seq type
        ast_settype(left, ast_type(ast_child(left)));

      return r;
    }

    case TK_TUPLE:
    {
      // Add a new node to the end of the path
      infer_path_t path_node = { 0, NULL, path->root };
      path->next = &path_node;

      for(ast_t* p = ast_child(left); p != NULL; p = ast_sibling(p))
      {
        infer_ret_t r = infer_local_inner(p, r_type, &path_node);

        if(r == INFER_ERROR)
          return INFER_ERROR;

        if(r == INFER_OK)
        {
          // Update tuple type element to remove infer type
          ast_t* old_ele = ast_childidx(ast_type(left), path_node.index);
          ast_replace(&old_ele, ast_type(p));
          ret_val = INFER_OK;
        }

        path_node.index++;
      }

      // Pop our node off the path
      path->next = NULL;
      return ret_val;
    }

    case TK_VAR:
    case TK_LET:
    {
      ast_t* var_type = ast_type(left);
      assert(var_type != NULL);

      if(ast_id(var_type) != TK_INFERTYPE)  // No inferring needed
        return INFER_NOP;

      ast_t* infer_type = find_infer_type(r_type, path->root->next);

      if(infer_type == NULL)
      {
        ast_error(left, "could not infer type of local");
        ast_settype(left, ast_from(left, TK_ERRORTYPE));
        return INFER_ERROR;
      }

      // Variable type is the alias of the inferred type
      ast_t* a_type = alias(infer_type);
      ast_settype(left, a_type);
      ast_settype(ast_child(left), a_type);

      // Add the type to the var / let AST as if it had been specified by the
      // user. Not really needed, but makes testing easier
      ast_t* speced_type = ast_childidx(left, 1);
      assert(ast_id(speced_type) == TK_NONE);
      ast_replace(&speced_type, a_type);

      ast_free_unattached(infer_type);
      return INFER_OK;
    }

    default:
      // No locals to infer here
      return INFER_NOP;
  }
}

static bool infer_locals(ast_t* left, ast_t* r_type)
{
  infer_path_t path_root = { 0, NULL, NULL };
  path_root.root = &path_root;

  if(infer_local_inner(left, r_type, &path_root) == INFER_ERROR)
    return false;

  assert(path_root.next == NULL);
  assert(path_root.root = &path_root);
  return true;
}

bool expr_assign(pass_opt_t* opt, ast_t* ast)
{
  // Left and right are swapped in the AST to make sure we type check the
  // right side before the left. Fetch them in the opposite order.
  assert(ast != NULL);

  AST_GET_CHILDREN(ast, right, left);
  ast_t* l_type = ast_type(left);

  if(!is_lvalue(&opt->check, left, is_result_needed(ast)))
  {
    ast_error(ast, "left side must be something that can be assigned to");
    return false;
  }

  assert(l_type != NULL);

  if(!coerce_literals(&right, l_type, opt))
    return false;

  ast_t* r_type = ast_type(right);

  if(is_typecheck_error(r_type))
    return false;

  if(!infer_locals(left, r_type))
    return false;

  // Inferring locals may have changed the left type.
  l_type = ast_type(left);

  // Assignment is based on the alias of the right hand side.
  ast_t* a_type = alias(r_type);

  if(!is_subtype(a_type, l_type, true))
  {
    ast_error(ast, "right side must be a subtype of left side");
    ast_error(a_type, "right side type: %s", ast_print_type(a_type));
    ast_error(l_type, "left side type: %s", ast_print_type(l_type));
    ast_free_unattached(a_type);
    return false;
  }

  if((ast_id(left) == TK_TUPLE) && (ast_id(a_type) != TK_TUPLETYPE))
  {
    switch(ast_id(a_type))
    {
      case TK_UNIONTYPE:
        ast_error(ast,
          "can't destructure a union using assignment, use pattern matching "
          "instead");
        break;

      case TK_ISECTTYPE:
        ast_error(ast,
          "can't destructure an intersection using assignment, use pattern "
          "matching instead");
        break;

      default:
        assert(0);
        break;
    }

    ast_error(a_type, "right side type: %s", ast_print_type(a_type));
    ast_free_unattached(a_type);
    return false;
  }

  bool ok_safe = safe_to_write(left, a_type);

  if(!ok_safe)
  {
    if(ast_id(left) == TK_FVARREF && ast_child(left) != NULL &&
      ast_id(ast_child(left)) == TK_THIS)
    {
      // We are writing to a field in this
      ast_t* fn = ast_nearest(left, TK_FUN);

      if(fn != NULL)
      {
        ast_t* iso = ast_child(fn);
        assert(iso != NULL);
        token_id iso_id = ast_id(iso);

        if(iso_id == TK_BOX || iso_id == TK_VAL || iso_id == TK_TAG)
        {
          ast_error(ast, "cannot write to a field in a %s function",
            lexer_print(iso_id));
          ast_free_unattached(a_type);
          return false;
        }
      }
    }

    ast_error(ast, "not safe to write right side to left side");
    ast_error(a_type, "right side type: %s", ast_print_type(a_type));
    ast_free_unattached(a_type);
    return false;
  }

  ast_free_unattached(a_type);

  // If it's an embedded field, check for a constructor result.
  if(ast_id(left) == TK_EMBEDREF)
  {
    if((ast_id(right) != TK_CALL) ||
      (ast_id(ast_childidx(right, 2)) != TK_NEWREF))
    {
      ast_error(ast, "an embedded field must be assigned using a constructor");
      return false;
    }
  }

  ast_settype(ast, consume_type(l_type, TK_NONE));
  ast_inheritflags(ast);
  return true;
}

bool expr_consume(typecheck_t* t, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, term);
  ast_t* type = ast_type(term);

  if(is_typecheck_error(type))
    return false;

  const char* name = NULL;

  switch(ast_id(term))
  {
    case TK_VARREF:
    case TK_LETREF:
    case TK_PARAMREF:
    {
      ast_t* id = ast_child(term);
      name = ast_name(id);
      break;
    }

    case TK_THIS:
    {
      name = stringtab("this");
      break;
    }

    default:
      ast_error(ast, "consume must take 'this', a local, or a parameter");
      return false;
  }

  // Can't consume from an outer scope while in a loop condition.
  if((t->frame->loop_cond != NULL) &&
    !ast_within_scope(t->frame->loop_cond, ast, name))
  {
    ast_error(ast, "can't consume from an outer scope in a loop condition");
    return false;
  }

  ast_setstatus(ast, name, SYM_CONSUMED);

  token_id tcap = ast_id(cap);
  ast_t* c_type = consume_type(type, tcap);

  if(c_type == NULL)
  {
    ast_error(ast, "can't consume to this capability");
    ast_error(term, "expression type is %s", ast_print_type(type));
    return false;
  }

  ast_settype(ast, c_type);
  return true;
}

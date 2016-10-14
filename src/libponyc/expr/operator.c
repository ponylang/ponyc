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

static bool assign_id(pass_opt_t* opt, ast_t* ast, bool let, bool need_value)
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
        ast_error(opt->check.errors, ast,
          "the left side is undefined but its value is used");
        return false;
      }

      ast_setstatus(ast, name, SYM_DEFINED);
      return true;

    case SYM_DEFINED:
      if(let)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let or embed definition more than once");
        return false;
      }

      return true;

    case SYM_CONSUMED:
    {
      bool ok = true;

      if(need_value)
      {
        ast_error(opt->check.errors, ast,
          "the left side is consumed but its value is used");
        ok = false;
      }

      if(let)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let or embed definition more than once");
        ok = false;
      }

      if(opt->check.frame->try_expr != NULL)
      {
        ast_error(opt->check.errors, ast,
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

static bool is_lvalue(pass_opt_t* opt, ast_t* ast, bool need_value)
{
  switch(ast_id(ast))
  {
    case TK_DONTCARE:
      // Can only assign to it if we don't need the value.
      return !need_value;

    case TK_VAR:
    case TK_LET:
      return assign_id(opt, ast_child(ast), ast_id(ast) == TK_LET, need_value);

    case TK_VARREF:
    {
      ast_t* id = ast_child(ast);
      return assign_id(opt, id, false, need_value);
    }

    case TK_LETREF:
    {
      ast_error(opt->check.errors, ast, "can't assign to a let local");
      return false;
    }

    case TK_FVARREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) == TK_THIS)
        return assign_id(opt, right, false, need_value);

      return true;
    }

    case TK_FLETREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
      {
        if(ast_id(ast_type(left)) == TK_TUPLETYPE)
        {
          ast_error(opt->check.errors, ast,
            "can't assign to an element of a tuple");
        } else {
          ast_error(opt->check.errors, ast, "can't assign to a let field");
        }
        return false;
      }

      if(opt->check.frame->loop_body != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let field in a loop");
        return false;
      }

      return assign_id(opt, right, true, need_value);
    }

    case TK_EMBEDREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
      {
        ast_error(opt->check.errors, ast, "can't assign to an embed field");
        return false;
      }

      if(opt->check.frame->loop_body != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to an embed field in a loop");
        return false;
      }

      return assign_id(opt, right, true, need_value);
    }

    case TK_TUPLE:
    {
      // A tuple is an lvalue if every component expression is an lvalue.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!is_lvalue(opt, child, need_value))
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

      return is_lvalue(opt, child, need_value);
    }

    default: {}
  }

  return false;
}

bool expr_identity(pass_opt_t* opt, ast_t* ast)
{
  ast_settype(ast, type_builtin(opt, ast, "Bool"));
  return literal_is(ast, opt);
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

static ast_t* find_infer_type(pass_opt_t* opt, ast_t* type, infer_path_t* path)
{
  assert(type != NULL);

  switch(ast_id(type))
  {
    case TK_TUPLETYPE:
      if(path == NULL)  // End of path, infer the whole tuple
        return type;

      if(path->index >= ast_childcount(type)) // Cardinality mismatch
        return NULL;

      return find_infer_type(opt, ast_childidx(type, path->index), path->next);

    case TK_UNIONTYPE:
    {
      // Infer all children
      ast_t* u_type = NULL;

      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
      {
        ast_t* t = find_infer_type(opt, p, path);

        if(t == NULL)
        {
          // Propogate error
          ast_free_unattached(u_type);
          return NULL;
        }

        u_type = type_union(opt, u_type, t);
      }

      return u_type;
    }

    case TK_ISECTTYPE:
    {
      // Infer all children
      ast_t* i_type = NULL;

      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
      {
        ast_t* t = find_infer_type(opt, p, path);

        if(t == NULL)
        {
          // Propogate error
          ast_free_unattached(i_type);
          return NULL;
        }

        i_type = type_isect(opt, i_type, t);
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

static infer_ret_t infer_local_inner(pass_opt_t* opt, ast_t* left,
  ast_t* r_type, infer_path_t* path)
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
      infer_ret_t r = infer_local_inner(opt, ast_child(left), r_type, path);

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
        infer_ret_t r = infer_local_inner(opt, p, r_type, &path_node);

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

      ast_t* infer_type = find_infer_type(opt, r_type, path->root->next);

      if(infer_type == NULL)
      {
        ast_error(opt->check.errors, left, "could not infer type of local");
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

static bool infer_locals(pass_opt_t* opt, ast_t* left, ast_t* r_type)
{
  infer_path_t path_root = { 0, NULL, NULL };
  path_root.root = &path_root;

  if(infer_local_inner(opt, left, r_type, &path_root) == INFER_ERROR)
    return false;

  assert(path_root.next == NULL);
  assert(path_root.root = &path_root);
  return true;
}

static bool is_expr_constructor(ast_t* ast)
{
  assert(ast != NULL);
  switch(ast_id(ast))
  {
    case TK_CALL:
      return ast_id(ast_childidx(ast, 2)) == TK_NEWREF;
    case TK_SEQ:
      return is_expr_constructor(ast_childlast(ast));
    case TK_IF:
    case TK_WHILE:
    case TK_REPEAT:
    {
      ast_t* body = ast_childidx(ast, 1);
      ast_t* else_expr = ast_childidx(ast, 2);
      return is_expr_constructor(body) && is_expr_constructor(else_expr);
    }
    case TK_TRY:
    {
      ast_t* body = ast_childidx(ast, 0);
      ast_t* else_expr = ast_childidx(ast, 1);
      return is_expr_constructor(body) && is_expr_constructor(else_expr);
    }
    case TK_MATCH:
    {
      ast_t* cases = ast_childidx(ast, 1);
      ast_t* else_expr = ast_childidx(ast, 2);
      ast_t* the_case = ast_child(cases);

      while(the_case != NULL)
      {
        if(!is_expr_constructor(ast_childidx(the_case, 2)))
          return false;
        the_case = ast_sibling(the_case);
      }
      return is_expr_constructor(else_expr);
    }
    case TK_RECOVER:
      return is_expr_constructor(ast_childidx(ast, 1));
    default:
      return false;
  }
}

static bool tuple_contains_embed(ast_t* ast)
{
  assert(ast_id(ast) == TK_TUPLE);
  ast_t* child = ast_child(ast);
  while(child != NULL)
  {
    if(ast_id(child) != TK_DONTCARE)
    {
      assert(ast_id(child) == TK_SEQ);
      ast_t* member = ast_childlast(child);
      if(ast_id(member) == TK_EMBEDREF)
      {
        return true;
      } else if(ast_id(member) == TK_TUPLE) {
        if(tuple_contains_embed(member))
          return true;
      }
    }
    child = ast_sibling(child);
  }
  return false;
}

static bool check_embed_construction(pass_opt_t* opt, ast_t* left, ast_t* right)
{
  bool result = true;
  if(ast_id(left) == TK_EMBEDREF)
  {
    if(!is_expr_constructor(right))
    {
      ast_error(opt->check.errors, left,
        "an embedded field must be assigned using a constructor");
      ast_error_continue(opt->check.errors, right,
        "the assigned expression is here");
      return false;
    }
  } else if(ast_id(left) == TK_TUPLE) {
    if(ast_id(right) == TK_TUPLE)
    {
      ast_t* l_child = ast_child(left);
      ast_t* r_child = ast_child(right);
      while(l_child != NULL)
      {
        if(ast_id(l_child) != TK_DONTCARE)
        {
          assert((ast_id(l_child) == TK_SEQ) && (ast_id(r_child) == TK_SEQ));
          ast_t* l_member = ast_childlast(l_child);
          ast_t* r_member = ast_childlast(r_child);
          if(!check_embed_construction(opt, l_member, r_member))
            result = false;
        }
        l_child = ast_sibling(l_child);
        r_child = ast_sibling(r_child);
      }
      assert(r_child == NULL);
    } else if(tuple_contains_embed(left)) {
      ast_error(opt->check.errors, left,
        "an embedded field must be assigned using a constructor");
      ast_error_continue(opt->check.errors, right,
        "the assigned expression isn't a tuple literal");
    }
  }
  return result;
}

bool expr_assign(pass_opt_t* opt, ast_t* ast)
{
  // Left and right are swapped in the AST to make sure we type check the
  // right side before the left. Fetch them in the opposite order.
  assert(ast != NULL);

  AST_GET_CHILDREN(ast, right, left);
  ast_t* l_type = ast_type(left);

  if(l_type == NULL || !is_lvalue(opt, left, is_result_needed(ast)))
  {
    ast_error(opt->check.errors, ast,
      "left side must be something that can be assigned to");
    return false;
  }

  if(!coerce_literals(&right, l_type, opt))
    return false;

  ast_t* r_type = ast_type(right);

  if(is_typecheck_error(r_type))
    return false;

  if(is_control_type(r_type))
  {
    ast_error(opt->check.errors, ast,
      "the right hand side does not return a value");
    return false;
  }

  if(!infer_locals(opt, left, r_type))
    return false;

  // Inferring locals may have changed the left type.
  l_type = ast_type(left);

  // Assignment is based on the alias of the right hand side.
  ast_t* a_type = alias(r_type);

  errorframe_t info = NULL;
  if(!is_subtype(a_type, l_type, &info, opt))
  {
    errorframe_t frame = NULL;
    ast_error_frame(&frame, ast, "right side must be a subtype of left side");
    errorframe_append(&frame, &info);
    errorframe_report(&frame, opt->check.errors);
    ast_free_unattached(a_type);
    return false;
  }

  if((ast_id(left) == TK_TUPLE) && (ast_id(a_type) != TK_TUPLETYPE))
  {
    switch(ast_id(a_type))
    {
      case TK_UNIONTYPE:
        ast_error(opt->check.errors, ast,
          "can't destructure a union using assignment, use pattern matching "
          "instead");
        break;

      case TK_ISECTTYPE:
        ast_error(opt->check.errors, ast,
          "can't destructure an intersection using assignment, use pattern "
          "matching instead");
        break;

      default:
        assert(0);
        break;
    }

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
          ast_error(opt->check.errors, ast,
            "cannot write to a field in a %s function. If you are trying to "
            "change state in a function use fun ref",
            lexer_print(iso_id));
          ast_free_unattached(a_type);
          return false;
        }
      }
    }

    ast_error(opt->check.errors, ast,
      "not safe to write right side to left side");
    ast_error_continue(opt->check.errors, a_type, "right side type: %s",
      ast_print_type(a_type));
    ast_free_unattached(a_type);
    return false;
  }

  ast_free_unattached(a_type);

  if(!check_embed_construction(opt, left, right))
    return false;

  ast_settype(ast, consume_type(l_type, TK_NONE));
  return true;
}

bool expr_consume(pass_opt_t* opt, ast_t* ast)
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
      ast_error(opt->check.errors, ast,
        "consume must take 'this', a local, or a parameter");
      return false;
  }

  // Can't consume from an outer scope while in a loop condition.
  if((opt->check.frame->loop_cond != NULL) &&
    !ast_within_scope(opt->check.frame->loop_cond, ast, name))
  {
    ast_error(opt->check.errors, ast,
      "can't consume from an outer scope in a loop condition");
    return false;
  }

  ast_setstatus(ast, name, SYM_CONSUMED);

  token_id tcap = ast_id(cap);
  ast_t* c_type = consume_type(type, tcap);

  if(c_type == NULL)
  {
    ast_error(opt->check.errors, ast, "can't consume to this capability");
    ast_error_continue(opt->check.errors, term, "expression type is %s",
      ast_print_type(type));
    return false;
  }

  ast_settype(ast, c_type);
  return true;
}

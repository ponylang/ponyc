#include "verify.h"
#include "../type/assemble.h"
#include "../type/cap.h"
#include "../type/lookup.h"
#include "../verify/call.h"
#include "../verify/control.h"
#include "../verify/fun.h"
#include "../verify/type.h"
#include "expr.h"
#include "../ast/ast.h"
#include "ponyassert.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>


static bool verify_assign_lvalue(pass_opt_t* opt, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_FLETREF:
    {
      AST_GET_CHILDREN(ast, left, right);

      if(ast_id(left) != TK_THIS)
      {
        ast_error(opt->check.errors, ast, "can't reassign to a let field");
        return false;
      }

      if(opt->check.frame->loop_body != NULL)
      {
        ast_error(opt->check.errors, ast,
          "can't assign to a let field in a loop");
        return false;
      }

      break;
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

      break;
    }

    case TK_TUPLEELEMREF:
    {
      ast_error(opt->check.errors, ast,
        "can't assign to an element of a tuple");
      return false;
    }

    case TK_TUPLE:
    {
      // Verify every lvalue in the tuple.
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        if(!verify_assign_lvalue(opt, child))
          return false;

        child = ast_sibling(child);
      }

      break;
    }

    case TK_SEQ:
    {
      // This is used because the elements of a tuple are sequences,
      // each sequence containing a single child.
      ast_t* child = ast_child(ast);
      pony_assert(ast_sibling(child) == NULL);

      return verify_assign_lvalue(opt, child);
    }

    default: {}
  }

  return true;
}

static size_t funref_hash(ast_t** key)
{
  return ponyint_hash_ptr(*key);
}

static bool funref_cmp(ast_t** a, ast_t** b)
{
  return *a == *b;
}

DECLARE_HASHMAP(consume_funs, consume_funs_t, ast_t*);

DEFINE_HASHMAP(consume_funs, consume_funs_t, ast_t*, funref_hash,
  funref_cmp, NULL);

// This function generates the fully qualified string (without the `this`) for a
// reference (i.e. `a.b.c.d.e`) and ensures it is part of the compiler
// `stringtab`. This is basically the same logic as the `generate_multi_dot_name`
// from `refer.c`. It would likely be possible to combine the two into a single
// function. It is used to generate this fully qualified string for the field
// being consumed for tracking its `consume`d status via the ast `symtab`. It is
// used to ensure that no parent (e.g. `a.b.c`) of a consumed field
// (e.g. `a.b.c.d.e`) is consumed.
static const char* get_multi_ref_name(ast_t* ast)
{
  ast_t* def = NULL;
  size_t len = 0;
  ast_t* temp_ast = ast;

  def = (ast_t*)ast_data(temp_ast);
  while(def == NULL)
  {
    AST_GET_CHILDREN(temp_ast, left, right);
    def = (ast_t*)ast_data(left);
    temp_ast = left;

    // the `+ 1` is for the '.' needed in the string
    len += strlen(ast_name(right)) + 1;
  }

  switch(ast_id(temp_ast))
  {
    case TK_DOT:
    {
      AST_GET_CHILDREN(temp_ast, left, right);
      if(ast_id(left) == TK_THIS)
      {
        temp_ast = right;
        len += strlen(ast_name(temp_ast));
      }
      else
        pony_assert(0);
      break;
    }

    case TK_FLETREF:
    case TK_FVARREF:
    case TK_EMBEDREF:
    case TK_REFERENCE:
    {
      temp_ast = ast_sibling(ast_child(temp_ast));
      len += strlen(ast_name(temp_ast));
      break;
    }

    case TK_PARAMREF:
    case TK_LETREF:
    case TK_VARREF:
    {
      temp_ast = ast_child(temp_ast);
      len += strlen(ast_name(temp_ast));
      break;
    }

    case TK_THIS:
    {
      temp_ast = ast_sibling(temp_ast);
      // string len already added by loop above
      // subtract 1 because we don't have to add '.'
      // since we're ignoring the `this`
      len -= 1;
      break;
    }

    default:
    {
      pony_assert(0);
    }
  }

  // for the \0 at the end
  len = len + 1;

  char* buf = (char*)ponyint_pool_alloc_size(len);
  size_t offset = 0;
  const char* name = ast_name(temp_ast);
  size_t slen = strlen(name);
  strncpy(buf + offset, name, slen);
  offset += slen;
  temp_ast = ast_parent(temp_ast);

  while(temp_ast != ast)
  {
    buf[offset] = '.';
    offset += 1;
    temp_ast = ast_sibling(temp_ast);
    name = ast_name(temp_ast);
    slen = strlen(name);
    strncpy(buf + offset, name, slen);
    offset += slen;
    temp_ast = ast_parent(temp_ast);
  }

  pony_assert((offset + 1) == len);
  buf[offset] = '\0';

  return stringtab_consume(buf, len);
}

static ast_t* get_fun_def(pass_opt_t* opt, ast_t* ast)
{
  pony_assert((ast_id(ast) == TK_FUNREF) || (ast_id(ast) == TK_FUNCHAIN) ||
    (ast_id(ast) == TK_NEWREF));
  AST_GET_CHILDREN(ast, receiver, method);

  // Receiver might be wrapped in another funref/newref
  // if the method had type parameters for qualification.
  if(ast_id(receiver) == ast_id(ast))
    AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);

  // Look up the original method definition for this method call.
  deferred_reification_t* method_def = lookup_try(opt, receiver,
    ast_type(receiver), ast_name(method), true);

  pony_assert(method_def != NULL);

  ast_t* method_ast = method_def->ast;
  deferred_reify_free(method_def);

  pony_assert(ast_id(method_ast) == TK_FUN || ast_id(method_ast) == TK_BE ||
    ast_id(method_ast) == TK_NEW);

  // behavior call
  if(ast_id(method_ast) == TK_BE)
    return NULL;
  else
    return method_ast;
}

static bool verify_fun_field_not_referenced(pass_opt_t* opt, ast_t* ast,
  ast_t* consume_ast, const char* origin_type_name, ast_t* consumed_type,
  const char* consumed_field_name, const char* consumed_field_full_name,
  consume_funs_t* consume_funs_map)
{
  token_id ast_token = ast_id(ast);
  if((ast_token == TK_FUNREF) || (ast_token == TK_NEWREF)
    || (ast_token == TK_FUNCHAIN))
  {
    ast_t* fun_def = get_fun_def(opt, ast);

    if(fun_def != NULL)
    {
      token_id entity_type = ast_id(ast_parent(ast_parent(fun_def)));
      if((entity_type == TK_TRAIT) || (entity_type == TK_INTERFACE))
      {
        ast_error(opt->check.errors, ast,
          "Cannot call functions on traits or interfaces after"
          " field consume or in any function called.");
        return false;
      }

      size_t index = HASHMAP_UNKNOWN;

      ast_t** fref = consume_funs_get(consume_funs_map, &fun_def, &index);

      // add to hashmap and evaluate if we haven't seen this function yet
      if(fref == NULL)
      {
        // add to hashmap
        consume_funs_putindex(consume_funs_map, &fun_def, index);

        // recursive check child
        // TODO: open question, print full function call tree?
        if(!verify_fun_field_not_referenced(opt, fun_def, consume_ast,
          origin_type_name, consumed_type, consumed_field_name,
          consumed_field_full_name, consume_funs_map))
          return false;
      }
    }
  }
  else if (ast_token == TK_FVARREF)
  {
    // check actual field definition to ensure it's safe
    const char* fname = ast_name(ast_sibling(ast_child(ast)));

    if(fname == consumed_field_name)
    {
      ast_t* child_ref = ast_child(ast);
      ast_t* ref_type = NULL;

      switch(ast_id(child_ref))
      {
        case TK_FVARREF:
        case TK_FLETREF:
        case TK_EMBEDREF:
        {
          ref_type = ast_type(ast_sibling(ast_child(child_ref)));
          break;
        }

        case TK_VARREF:
        case TK_LETREF:
        case TK_THIS:
        {
          ref_type = ast_type(child_ref);
          break;
        }

        default:
        {
          pony_assert(0);
          return false;
        }
      }

      const char* consumed_type_name =
        ast_name(ast_sibling(ast_child(consumed_type)));
      const char* ref_type_name = ast_name(ast_sibling(ast_child(ref_type)));

      if(consumed_type_name == ref_type_name)
      {
        ast_error(opt->check.errors, ast,
          "Cannot reference consumed field or a field of the same type after"
          " consume or in any function called.");
        return false;
      }
    }
  }
  else if (ast_token == TK_CONSUME)
  {
    if(ast == consume_ast)
      return true;

    ast_t* cfield = ast_sibling(ast_child(ast));

    if((ast_id(cfield) == TK_FVARREF) || (ast_id(cfield) == TK_FLETREF)
      || (ast_id(cfield) == TK_EMBEDREF))
    {
      ast_t* temp_ast = cfield;

      const char* ctype_name = NULL;

      do
      {
        if(ast_id(temp_ast) == TK_THIS)
           ctype_name = ast_name(ast_sibling(ast_child(ast_type(temp_ast))));

        temp_ast = ast_child(temp_ast);
      } while(temp_ast != NULL);

      // ensure we're in the same object type as original consume
      if((ctype_name != NULL) && (ctype_name == origin_type_name))
      {
        const char* cname = get_multi_ref_name(cfield);

        if(strncmp(cname, consumed_field_full_name, strlen(cname)) == 0)
        {
          ast_error(opt->check.errors, ast,
            "Cannot consume parent object of a consumed field in function call"
            " tree.");
          return false;
        }
      }
    }
  }

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // recursive check child
    if(!verify_fun_field_not_referenced(opt, child, consume_ast,
      origin_type_name, consumed_type, consumed_field_name,
      consumed_field_full_name, consume_funs_map))
      return false;

    // next child
    child = ast_sibling(child);
  }

  return true;
}

static bool verify_consume_field_not_referenced(pass_opt_t* opt,
  ast_t* assign_ast, ast_t* ast)
{
  if(!ast_checkflag(ast, AST_FLAG_FCNSM_REASGN))
    return true;

  token_id tk = ast_id(ast);

  // found consume; extract type/field name and check if used anywhere in
  // function calls between here and assignment
  if(tk == TK_CONSUME)
  {
    consume_funs_t* consume_funs_map =
      (consume_funs_t*)POOL_ALLOC(consume_funs_t);
    memset(consume_funs_map, 0, sizeof(consume_funs_t));

    ast_t* cfield = ast_sibling(ast_child(ast));
    ast_t* consumed_type = ast_type(ast_child(cfield));
    const char* consumed_field = ast_name(ast_sibling(ast_child(cfield)));

    const char* cname = get_multi_ref_name(cfield);

    const char* origin_type_name = ast_name(ast_child(opt->check.frame->type));

    ast_t* consume_ast = ast;

    ast_t* parent = ast_parent(ast);

    while(parent != assign_ast)
    {
      ast = parent;
      parent = ast_parent(ast);
    }

    while(ast != NULL)
    {
      if(!verify_fun_field_not_referenced(opt, ast, consume_ast,
        origin_type_name, consumed_type, consumed_field, cname,
        consume_funs_map))
      {
        consume_funs_destroy(consume_funs_map);
        POOL_FREE(consume_funs_t, consume_funs_map);

        ast_error_continue(opt->check.errors, consume_ast,
          "field consumed here");
        return false;
      }

      ast = ast_sibling(ast);
    }

    consume_funs_destroy(consume_funs_map);
    POOL_FREE(consume_funs_t, consume_funs_map);

    return true;
  }

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // recursive check child
    if(!verify_consume_field_not_referenced(opt, assign_ast, child))
      return false;

    // next child
    child = ast_sibling(child);
  }

  // no errors in any children or this node
  return true;
}

static bool verify_reassign_consumed(pass_opt_t* opt, ast_t* ast)
{
  uint32_t ast_flags = ast_checkflag(ast,
    AST_FLAG_CAN_ERROR | AST_FLAG_CNSM_REASGN | AST_FLAG_FCNSM_REASGN);

  if(!(ast_flags & (AST_FLAG_CNSM_REASGN | AST_FLAG_FCNSM_REASGN)))
    return true;

  // if it's a consume/reassign of a field and an error can be thrown
  if(ast_flags == (AST_FLAG_CAN_ERROR | AST_FLAG_FCNSM_REASGN))
  {
    ast_error(opt->check.errors, ast,
      "can't reassign to a consumed field in an expression if there is a partial"
      " call involved");
    return false;
  }
  // if it's a consume/reassign of a field, check to ensure field is not
  // referenced by a function call
  else if(ast_flags & AST_FLAG_FCNSM_REASGN)
  {
    if(!verify_consume_field_not_referenced(opt, ast, ast))
      return false;
  }
  // if it's a consume/reassign and an error can be thrown
  // and we're in a try
  else if((ast_flags == (AST_FLAG_CAN_ERROR | AST_FLAG_CNSM_REASGN))
    && (opt->check.frame->try_expr != NULL))
  {
    ast_error(opt->check.errors, ast,
      "can't reassign to a consumed identifier in a try expression if there is a"
      " partial call involved");
    return false;
  }

  return true;
}


static bool verify_assign(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_ASSIGN);
  AST_GET_CHILDREN(ast, left, right);

  if(!verify_assign_lvalue(opt, left))
    return false;

  ast_inheritflags(ast);

  // Reassign of consumed identifiers only allowed if there are no partial
  // calls involved
  if(!verify_reassign_consumed(opt, ast))
    return false;

  return true;
}


ast_result_t pass_verify(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
    case TK_STRUCT:       r = verify_struct(options, ast); break;
    case TK_ASSIGN:       r = verify_assign(options, ast); break;
    case TK_FUN:
    case TK_NEW:
    case TK_BE:           r = verify_fun(options, ast); break;
    case TK_FUNREF:
    case TK_FUNCHAIN:
    case TK_NEWREF:       r = verify_function_call(options, ast); break;
    case TK_BEREF:
    case TK_BECHAIN:
    case TK_NEWBEREF:     r = verify_behaviour_call(options, ast); break;
    case TK_FFICALL:      r = verify_ffi_call(options, ast); break;
    case TK_TRY:
    case TK_TRY_NO_CHECK: r = verify_try(options, ast); break;
    case TK_ERROR:        ast_seterror(ast); break;

    default:              ast_inheritflags(ast); break;
  }

  if(!r)
  {
    pony_assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}

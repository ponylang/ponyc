#include "verify.h"
#include "../verify/call.h"
#include "../verify/control.h"
#include "../verify/fun.h"
#include "expr.h" // TODO: remove when is_result_needed is moved?
#include "ponyassert.h"


static bool is_legal_dontcare_read(ast_t* ast)
{
  // We either are the LHS of an assignment or a tuple element. That tuple must
  // either be a pattern or the LHS of an assignment. It can be embedded in
  // other tuples, which may appear in sequences.

  // '_' may be wrapped in a sequence.
  ast_t* parent = ast_parent(ast);
  if(ast_id(parent) == TK_SEQ)
    parent = ast_parent(parent);

  switch(ast_id(parent))
  {
    case TK_ASSIGN:
    {
      AST_GET_CHILDREN(parent, right, left);
      if(ast == left)
        return true;
      return false;
    }

    case TK_TUPLE:
    {
      ast_t* grandparent = ast_parent(parent);

      while((ast_id(grandparent) == TK_TUPLE) ||
        (ast_id(grandparent) == TK_SEQ))
      {
        parent = grandparent;
        grandparent = ast_parent(parent);
      }

      switch(ast_id(grandparent))
      {
        case TK_ASSIGN:
        {
          AST_GET_CHILDREN(grandparent, right, left);

          if(parent == left)
            return true;

          break;
        }

        case TK_CASE:
        {
          AST_GET_CHILDREN(grandparent, pattern, guard, body);

          if(parent == pattern)
            return true;

          break;
        }

        default: {}
      }

      break;
    }

    default: {}
  }

  return false;
}


bool verify_dontcareref(pass_opt_t* opt, ast_t* ast)
{
  if(is_result_needed(ast) && !is_legal_dontcare_read(ast))
  {
    ast_error(opt->check.errors, ast, "can't read from '_'");
    return false;
  }

  return true;
}


ast_result_t pass_verify(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;
  bool r = true;

  switch(ast_id(ast))
  {
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
    case TK_DONTCAREREF:  r = verify_dontcareref(options, ast); break;
    case TK_LETREF:
    case TK_VARREF:
    case TK_FLETREF:
    case TK_FVARREF:
    case TK_EMBEDREF:
    case TK_TUPLEELEMREF:
    case TK_CALL:
    case TK_QUALIFY:
    case TK_TUPLE:
    case TK_ASSIGN:
    case TK_MATCH:
    case TK_CASES:
    case TK_CASE:
    case TK_IS:
    case TK_ISNT:
    case TK_SEQ:
    case TK_BREAK:
    case TK_RETURN:
    case TK_IF:
    case TK_IFDEF:
    case TK_WHILE:
    case TK_REPEAT:
    case TK_RECOVER:
    case TK_POSITIONALARGS:
    case TK_NAMEDARGS:
    case TK_NAMEDARG:
    case TK_UPDATEARG:    ast_inheritflags(ast); break;
    case TK_ERROR:        ast_seterror(ast); break;

    default: {}
  }

  if(!r)
  {
    pony_assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}

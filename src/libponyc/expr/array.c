#include "array.h"
#include "reference.h"
#include "postfix.h"
#include "call.h"
#include "../ast/astbuild.h"
#include "../expr/literal.h"
#include "../pkg/package.h"
#include "../pass/names.h"
#include "../pass/expr.h"
#include "../pass/refer.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/subtype.h"

bool expr_array(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;
  ast_t* type = NULL;
  bool told_type = false;

  AST_GET_CHILDREN(ast, type_spec, elements);
  size_t size = ast_childcount(elements);

  if(ast_id(type_spec) != TK_NONE)
  {
    type = type_spec;
    told_type = true;
  }

  for(ast_t* ele = ast_child(elements); ele != NULL; ele = ast_sibling(ele))
  {
    if(ast_checkflag(ele, AST_FLAG_JUMPS_AWAY))
    {
      ast_error(opt->check.errors, ele,
          "an array can't contain an expression that jumps away with no value");
      ast_free_unattached(type);
      return false;
    }

    ast_t* c_type = ast_type(ele);
    if(is_typecheck_error(c_type))
      return false;

    if(told_type)
    {
      // Don't need to free type here on exit
      if(!coerce_literals(&ele, type, opt))
        return false;

      c_type = ast_type(ele); // May have changed due to literals

      errorframe_t info = NULL;
      if(!is_subtype(c_type, type, &info, opt))
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, ele,
          "array element not a subtype of specified array type");
        ast_error_frame(&frame, type_spec, "array type: %s",
          ast_print_type(type));
        ast_error_frame(&frame, c_type, "element type: %s",
          ast_print_type(c_type));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
        return false;
      }
    }
    else
    {
      if(is_type_literal(c_type))
      {
        // At least one array element is literal, so whole array is
        ast_free_unattached(type);
        make_literal_type(ast);
        return true;
      }

      type = type_union(opt, type, c_type);
    }
  }

  BUILD(ref, ast, NODE(TK_REFERENCE, ID("Array")));

  ast_t* a_type = alias(type);

  BUILD(qualify, ast,
    NODE(TK_QUALIFY,
      TREE(ref)
      NODE(TK_TYPEARGS, TREE(a_type))));

  ast_free_unattached(type);

  BUILD(dot, ast, NODE(TK_DOT, TREE(qualify) ID("create")));

  ast_t* size_arg = ast_from_int(ast, size);
  BUILD(size_arg_seq, ast, NODE(TK_SEQ, TREE(size_arg)));
  ast_settype(size_arg, type_builtin(opt, ast, "USize"));
  ast_settype(size_arg_seq, type_builtin(opt, ast, "USize"));

  BUILD(call, ast,
    NODE(TK_CALL,
      NODE(TK_POSITIONALARGS, TREE(size_arg_seq))
      NONE
      NONE
      TREE(dot)));

  if(!refer_reference(opt, &ref) ||
    !refer_qualify(opt, qualify) ||
    !expr_typeref(opt, &qualify) ||
    !expr_dot(opt, &dot) ||
    !expr_call(opt, &call)
    )
    return false;

  ast_swap(ast, call);
  *astp = call;

  elements = ast_childidx(ast, 1);

  for(ast_t* ele = ast_child(elements); ele != NULL; ele = ast_sibling(ele))
  {
    BUILD(append_chain, ast, NODE(TK_CHAIN, TREE(*astp) ID("push")));

    BUILD(append, ast,
      NODE(TK_CALL,
        NODE(TK_POSITIONALARGS, TREE(ele))
        NONE
        NONE
        TREE(append_chain)));

    ast_replace(astp, append);

    if(!expr_chain(opt, &append_chain) ||
      !expr_call(opt, &append)
      )
      return false;
  }

  ast_free_unattached(ast);
  return true;
}

#include "array.h"
#include "reference.h"
#include "postfix.h"
#include "call.h"
#include "../ast/astbuild.h"
#include "../expr/literal.h"
#include "../pkg/package.h"
#include "../pass/names.h"
#include "../pass/expr.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/subtype.h"

bool expr_array(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;
  size_t size = ast_childcount(ast) - 1;
  ast_t* type = NULL;
  bool told_type = false;

  AST_GET_CHILDREN(ast, type_spec, first_child);

  if(ast_id(type_spec) != TK_NONE)
  {
    type = type_spec;
    told_type = true;
  }

  for(ast_t* ele = first_child; ele != NULL; ele = ast_sibling(ele))
  {
    ast_t* c_type = ast_type(ele);

    if(is_control_type(c_type))
    {
      ast_error(opt->check.errors, ele,
        "can't use an expression without a value in an array constructor");
      ast_free_unattached(type);
      return false;
    }

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
      TREE(dot)));

  if(!expr_reference(opt, &ref) ||
    !expr_qualify(opt, &qualify) ||
    !expr_dot(opt, &dot) ||
    !expr_call(opt, &call)
    )
    return false;

  ast_swap(ast, call);
  *astp = call;

  for(ast_t* ele = ast_childidx(ast, 1); ele != NULL; ele = ast_sibling(ele))
  {
    BUILD(append_dot, ast, NODE(TK_DOT, TREE(*astp) ID("push")));

    BUILD(append, ast,
      NODE(TK_CALL,
        NODE(TK_POSITIONALARGS, TREE(ele))
        NONE
        TREE(append_dot)));

    ast_replace(astp, append);

    if(!expr_dot(opt, &append_dot) ||
      !expr_call(opt, &append)
      )
      return false;
  }

  ast_free_unattached(ast);
  return true;
}

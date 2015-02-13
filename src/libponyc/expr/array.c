#include "array.h"
#include "reference.h"
#include "postfix.h"
#include "call.h"
#include "../expr/literal.h"
#include "../pkg/package.h"
#include "../pass/names.h"
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

    if(c_type == NULL)
    {
      ast_error(ele,
        "can't use an expression without a value in an array constructor");
      ast_free_unattached(type);
      return false;
    }

    if(told_type)
    {
      // Don't need to free type here on exit
      if(!coerce_literals(&ele, type, opt))
        return false;

      c_type = ast_type(ele); // May have changed due to literals

      if(!is_subtype(c_type, type))
      {
        ast_error(ele, "array element not a subtype of specified array type");
        ast_error(type_spec, "array type: %s", ast_print_type(type));
        ast_error(c_type, "element type: %s", ast_print_type(c_type));
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

      type = type_union(type, c_type);
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
  ast_settype(size_arg, type_builtin(opt, ast, "U64"));

  BUILD(call, ast,
    NODE(TK_CALL,
      NODE(TK_POSITIONALARGS, TREE(size_arg))
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
    BUILD(append_dot, ast, NODE(TK_DOT, TREE(*astp) ID("append")));

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

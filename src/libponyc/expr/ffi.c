#include "ffi.h"
#include "literal.h"
#include "../type/subtype.h"
#include "../pkg/ifdef.h"
#include "../pass/expr.h"
#include "../type/alias.h"
#include "ponyassert.h"
#include <string.h>

bool void_star_param(ast_t* param_type, ast_t* arg_type)
{
  pony_assert(param_type != NULL);
  pony_assert(arg_type != NULL);

  if(!is_pointer(param_type))
    return false;

  ast_t* type_args = ast_childidx(param_type, 2);

  if(ast_childcount(type_args) != 1 || !is_none(ast_child(type_args)))
    return false;

  // Parameter type is Pointer[None]
  // If the argument is Pointer[A], NullablePointer[A] or USize, allow it
  while(ast_id(arg_type) == TK_ARROW)
    arg_type = ast_childidx(arg_type, 1);

  if(is_pointer(arg_type) ||
    is_nullable_pointer(arg_type) ||
    is_literal(arg_type, "USize"))
    return true;

  return false;
}

static bool declared_ffi(pass_opt_t* opt, ast_t* call, ast_t* decl)
{
  pony_assert(call != NULL);
  pony_assert(decl != NULL);
  pony_assert(ast_id(decl) == TK_FFIDECL);

  AST_GET_CHILDREN(call, call_name, call_ret_typeargs, args, named_args, call_error);
  AST_GET_CHILDREN(decl, decl_name, decl_ret_typeargs, params, named_params,
    decl_error);

  // Check args vs params
  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(args);

  while((arg != NULL) && (param != NULL) && ast_id(param) != TK_ELLIPSIS)
  {
    ast_t* p_type = ast_childidx(param, 1);

    if(!coerce_literals(&arg, p_type, opt))
      return false;

    ast_t* arg_type = ast_type(arg);

    if(is_typecheck_error(arg_type))
      return false;

    ast_t* a_type = alias(arg_type);
    errorframe_t info = NULL;

    if(!void_star_param(p_type, a_type) &&
      !is_subtype(a_type, p_type, &info, opt))
    {
      errorframe_t frame = NULL;
      ast_error_frame(&frame, arg, "argument not a assignable to parameter");
      ast_error_frame(&frame, arg, "argument type is %s",
                      ast_print_type(a_type));
      ast_error_frame(&frame, param, "parameter type requires %s",
                      ast_print_type(p_type));
      errorframe_append(&frame, &info);
      errorframe_report(&frame, opt->check.errors);
      ast_free_unattached(a_type);
      return false;
    }

    ast_free_unattached(a_type);
    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  if(arg != NULL && param == NULL)
  {
    ast_error(opt->check.errors, arg, "too many arguments");
    return false;
  }

  if(param != NULL && ast_id(param) != TK_ELLIPSIS)
  {
    ast_error(opt->check.errors, named_args, "too few arguments");
    return false;
  }

  for(; arg != NULL; arg = ast_sibling(arg))
  {
    ast_t* a_type = ast_type(arg);

    if((a_type != NULL) && is_type_literal(a_type))
    {
      ast_error(opt->check.errors, arg,
        "Cannot pass number literals as unchecked FFI arguments");
      return false;
    }
  }

  // Check return types
  ast_t* call_ret_type = ast_child(call_ret_typeargs);
  ast_t* decl_ret_type = ast_child(decl_ret_typeargs);

  // Return types at the call site completely override any types
  // found at the declaration
  ast_t* ret_type;
  if(call_ret_type != NULL)
    ret_type = call_ret_type;
  else
    ret_type = decl_ret_type;

  // Store the declaration so that codegen can generate a non-variadic
  // signature for the FFI call.
  ast_setdata(call, decl);
  ast_settype(call, ret_type);
  return true;
}

bool expr_ffi(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, name, return_typeargs, args, namedargs, question);
  pony_assert(name != NULL);

  ast_t* decl;
  if(!ffi_get_decl(&opt->check, ast, &decl, opt))
    return false;

  if(decl == NULL)
  {
    ast_error(opt->check.errors, ast_child(ast),
      "An FFI call needs a declaration");
    return false;
  }

  return declared_ffi(opt, ast, decl);
}

#include "ffi.h"
#include "literal.h"
#include "../type/subtype.h"
#include "../pkg/ifdef.h"
#include "../pkg/package.h"
#include <assert.h>

static bool void_star_param(ast_t* param_type, ast_t* arg_type)
{
  assert(param_type != NULL);
  assert(arg_type != NULL);

  if(!is_pointer(param_type))
    return false;

  ast_t* type_args = ast_childidx(param_type, 2);

  if(ast_childcount(type_args) != 1 || !is_none(ast_child(type_args)))
    return false;

  // Parameter type is Pointer[None]
  // If the argument is Pointer[A], MaybePointer[A] or USize, allow it
  while(ast_id(arg_type) == TK_ARROW)
    arg_type = ast_childidx(arg_type, 1);

  if(is_pointer(arg_type) ||
    is_maybe(arg_type) ||
    is_literal(arg_type, "USize"))
    return true;

  return false;
}

static ast_result_t declared_ffi(pass_opt_t* opt, ast_t* call, ast_t* decl)
{
  assert(call != NULL);
  assert(decl != NULL);
  assert(ast_id(decl) == TK_FFIDECL);

  AST_GET_CHILDREN(call, call_name, call_ret_typeargs, args, named_args,
    call_error);
  AST_GET_CHILDREN(decl, decl_name, decl_ret_typeargs, params, named_params,
    decl_error);

  // Check args vs params
  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(args);

  while((arg != NULL) && (param != NULL) && ast_id(param) != TK_ELLIPSIS)
  {
    ast_t* p_type = ast_childidx(param, 1);

    if(!coerce_literals(&arg, p_type, opt))
      return AST_ERROR;

    ast_t* a_type = ast_type(arg);

    errorframe_t info = NULL;
    if((a_type != NULL) &&
      !void_star_param(p_type, a_type) &&
      !is_subtype(a_type, p_type, &info, opt))
    {
      errorframe_t frame = NULL;
      ast_error_frame(&frame, arg, "argument not a subtype of parameter");
      ast_error_frame(&frame, param, "parameter type: %s",
        ast_print_type(p_type));
      ast_error_frame(&frame, arg, "argument type: %s",
        ast_print_type(a_type));
      errorframe_append(&frame, &info);
      errorframe_report(&frame, opt->check.errors);
      return AST_ERROR;
    }

    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  if(arg != NULL && param == NULL)
  {
    ast_error(opt->check.errors, arg, "too many arguments");
    return AST_ERROR;
  }

  if(param != NULL && ast_id(param) != TK_ELLIPSIS)
  {
    ast_error(opt->check.errors, named_args, "too few arguments");
    return AST_ERROR;
  }

  for(; arg != NULL; arg = ast_sibling(arg))
  {
    ast_t* a_type = ast_type(arg);

    if((a_type != NULL) && is_type_literal(a_type))
    {
      ast_error(opt->check.errors, arg,
        "Cannot pass number literals as unchecked FFI arguments");
      return AST_ERROR;
    }
  }

  // Check return types
  ast_t* call_ret_type = ast_child(call_ret_typeargs);
  ast_t* decl_ret_type = ast_child(decl_ret_typeargs);

  errorframe_t info = NULL;
  if((call_ret_type != NULL) &&
    !is_eqtype(call_ret_type, decl_ret_type, &info, opt))
  {
    errorframe_t frame = NULL;
    ast_error_frame(&frame, call_ret_type,
      "call return type does not match declaration");
    errorframe_append(&frame, &info);
    errorframe_report(&frame, opt->check.errors);
    return AST_ERROR;
  }

  // Check partiality
  if((ast_id(decl_error) == TK_NONE) && (ast_id(call_error) != TK_NONE))
  {
    ast_error(opt->check.errors, call_error,
      "call is partial but the declaration is not");
    return AST_ERROR;
  }

  if((ast_id(decl_error) == TK_QUESTION) ||
    (ast_id(call_error) == TK_QUESTION))
  {
    ast_seterror(call);
  }

  // Store the declaration so that codegen can generate a non-variadic
  // signature for the FFI call.
  ast_setdata(call, decl);
  ast_settype(call, decl_ret_type);
  ast_inheritflags(call);
  return AST_OK;
}

ast_result_t expr_ffi(pass_opt_t* opt, ast_t* ast)
{
  if(!package_allow_ffi(&opt->check))
  {
    ast_error(opt->check.errors, ast, "This package isn't allowed to do C FFI");
    return AST_FATAL;
  }

  AST_GET_CHILDREN(ast, name, return_typeargs, args, namedargs, question);
  assert(name != NULL);

  ast_t* decl;
  if(!ffi_get_decl(&opt->check, ast, &decl, opt))
    return AST_ERROR;

  if(decl != NULL)  // We have a declaration
    return declared_ffi(opt, ast, decl);

  // We do not have a declaration
  for(ast_t* arg = ast_child(args); arg != NULL; arg = ast_sibling(arg))
  {
    ast_t* a_type = ast_type(arg);

    if((a_type != NULL) && is_type_literal(a_type))
    {
      ast_error(opt->check.errors, arg,
        "Cannot pass number literals as unchecked FFI arguments");
      return AST_ERROR;
    }
  }

  ast_t* return_type = ast_child(return_typeargs);

  if(return_type == NULL)
  {
    ast_error(opt->check.errors, name,
      "FFIs without declarations must specify return type");
    return AST_ERROR;
  }

  ast_settype(ast, return_type);
  ast_inheritflags(ast);

  if(ast_id(question) == TK_QUESTION)
    ast_seterror(ast);

  return AST_OK;
}

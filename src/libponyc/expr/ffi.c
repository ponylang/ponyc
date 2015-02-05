#include "ffi.h"
#include "literal.h"
#include "../type/subtype.h"
#include <assert.h>

static bool declared_ffi(pass_opt_t* opt, ast_t* call, ast_t* decl)
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
      return false;

    ast_t* a_type = ast_type(arg);

    if((a_type != NULL) && !is_subtype(a_type, p_type))
    {
      ast_error(arg, "argument not a subtype of parameter");
      ast_error(param, "parameter type: %s", ast_print_type(p_type));
      ast_error(arg, "argument type: %s", ast_print_type(a_type));
      return false;
    }

    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  if(arg != NULL && param == NULL)
  {
    ast_error(arg, "too many arguments");
    return false;
  }

  if(param != NULL && ast_id(param) != TK_ELLIPSIS)
  {
    ast_error(named_args, "too few arguments");
    return false;
  }

  for(; arg != NULL; arg = ast_sibling(arg))
  {
    ast_t* a_type = ast_type(arg);

    if((a_type != NULL) && is_type_literal(a_type))
    {
      ast_error(arg, "Cannot pass number literals as unchecked FFI arguments");
      return false;
    }
  }

  // Check return types
  ast_t* call_ret_type = ast_child(call_ret_typeargs);
  ast_t* decl_ret_type = ast_child(decl_ret_typeargs);

  if(call_ret_type != NULL && !is_eqtype(call_ret_type, decl_ret_type))
  {
    ast_error(call_ret_type, "call return type does not match declaration");
    return false;
  }

  // Check partiality
  if((ast_id(decl_error) == TK_NONE) && (ast_id(call_error) != TK_NONE))
  {
    ast_error(call_error, "call is partial but the declaration is not");
    return false;
  }

  if((ast_id(decl_error) == TK_QUESTION) ||
    (ast_id(call_error) == TK_QUESTION))
  {
    ast_seterror(call);
  }

  ast_settype(call, decl_ret_type);
  return true;
}

bool expr_ffi(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, name, return_typeargs, args, namedargs, question);
  assert(name != NULL);

  ast_t* decl = ast_get(ast, ast_name(name), NULL);

  if(decl != NULL)  // We have a declaration
    return declared_ffi(opt, ast, decl);

  // We do not have a declaration
  for(ast_t* arg = ast_child(args); arg != NULL; arg = ast_sibling(arg))
  {
    ast_t* a_type = ast_type(arg);

    if((a_type != NULL) && is_type_literal(a_type))
    {
      ast_error(arg, "Cannot pass number literals as unchecked FFI arguments");
      return false;
    }
  }

  ast_t* return_type = ast_child(return_typeargs);

  if(return_type == NULL)
  {
    ast_error(name, "FFIs without declarations must specify return type");
    return false;
  }

  ast_settype(ast, return_type);

  if(ast_id(question) == TK_QUESTION)
    ast_seterror(ast);

  return true;
}

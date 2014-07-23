#include "genfun.h"
#include "gentype.h"
#include "genname.h"
#include "../type/reify.h"
#include "../type/lookup.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static void name_params(ast_t* fun, ast_t* params, LLVMValueRef func)
{
  int count = 0;

  // name the receiver 'this'
  if(ast_id(fun) != TK_NEW)
  {
    LLVMValueRef fparam = LLVMGetParam(func, count++);
    LLVMSetValueName(fparam, "this");
  }

  // name each parameter
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    LLVMValueRef fparam = LLVMGetParam(func, count++);
    LLVMSetValueName(fparam, ast_name(ast_child(param)));
    param = ast_sibling(param);
  }
}

LLVMValueRef get_prototype(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs)
{
  // get a fully qualified name: starts with the type name, followed by the
  // type arguments, followed by the function name, followed by the function
  // level type arguments.
  const char* type_name = genname_type(type);
  const char* funname = genname_fun(type_name, name, typeargs);

  // if the function already exists, just return it
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func != NULL)
    return func;

  // reify with both the type and the function-level typeargs
  ast_t* fun = lookup(type, name);
  assert(fun != NULL);

  if(typeargs != NULL)
  {
    ast_t* typeparams = ast_childidx(fun, 2);
    ast_t* r_fun = reify(fun, typeparams, typeargs);
    ast_free_unattached(fun);
    fun = r_fun;
    assert(fun != NULL);
  }

  // count the parameters
  ast_t* params = ast_childidx(fun, 3);
  size_t count = ast_childcount(params);

  // if we're not a constructor, the receiver is another parameter
  if(ast_id(fun) != TK_NEW)
    count++;

  LLVMTypeRef tparams[count];
  count = 0;

  // get a type for the receiver
  if(ast_id(fun) != TK_NEW)
  {
    tparams[count] = gentype(c, type);

    if(tparams[count] == NULL)
    {
      ast_error(fun, "couldn't generate receiver type");
      return NULL;
    }

    count++;
  }

  // get a type for each parameter
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* ptype = ast_childidx(param, 1);
    tparams[count] = gentype(c, ptype);

    if(tparams[count] == NULL)
    {
      ast_error(ptype, "couldn't generate parameter type");
      return NULL;
    }

    count++;
    param = ast_sibling(param);
  }

  // get a type for the result
  ast_t* rtype = ast_childidx(fun, 4);
  LLVMTypeRef result = gentype(c, rtype);

  if(result == NULL)
  {
    ast_error(rtype, "couldn't generate result type");
    return NULL;
  }

  // generate the function type and the function prototype
  LLVMTypeRef ftype = LLVMFunctionType(result, tparams, count, false);
  func = LLVMAddFunction(c->module, funname, ftype);
  name_params(fun, params, func);

  if((ast_id(fun) == TK_BE) ||
    ((ast_id(ast_data(type)) == TK_ACTOR) && (ast_id(fun) == TK_NEW)))
  {
    const char* handler_name = genname_handler(type_name, name, typeargs);
    LLVMValueRef handler = LLVMAddFunction(c->module, handler_name, ftype);
    name_params(fun, params, handler);
  }

  return func;
}

LLVMValueRef genfun(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs)
{
  LLVMValueRef fun = get_prototype(c, type, name, typeargs);

  if(fun == NULL)
    return NULL;

  // TODO: body and finish
  return fun;
}

LLVMValueRef genfun_be(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, int index)
{
  LLVMValueRef fun = get_prototype(c, type, name, typeargs);

  if(fun == NULL)
    return NULL;

  // TODO: body and finish

  const char* handler_name = genname_handler(genname_type(type), name,
    typeargs);
  LLVMValueRef handler = LLVMGetNamedFunction(c->module, handler_name);

  if(handler == NULL)
    return NULL;

  // TODO: body and finish
  return fun;
}

LLVMValueRef genfun_new(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs)
{
  LLVMValueRef fun = get_prototype(c, type, name, typeargs);

  if(fun == NULL)
    return NULL;

  // TODO: body and finish
  return fun;
}

LLVMValueRef genfun_newbe(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, int index)
{
  LLVMValueRef fun = get_prototype(c, type, name, typeargs);

  if(fun == NULL)
    return NULL;

  // TODO: body and finish

  const char* handler_name = genname_handler(genname_type(type), name,
    typeargs);
  LLVMValueRef handler = LLVMGetNamedFunction(c->module, handler_name);

  if(handler == NULL)
    return NULL;

  // TODO: body and finish
  return fun;
}

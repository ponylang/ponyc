#include "genfun.h"
#include "gentype.h"
#include "genname.h"
#include "../type/reify.h"
#include "../type/lookup.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

LLVMValueRef codegen_function(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs)
{
  const char* funname = codegen_funname(type, name, typeargs);

  if(funname == NULL)
  {
    ast_error(type, "couldn't generate function name for '%s'", name);
    return NULL;
  }

  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func != NULL)
    return func;

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

  ast_t* params = ast_childidx(fun, 3);
  size_t count = ast_childcount(params) + 1;

  if(ast_id(fun) != TK_NEW)
    count++;

  LLVMTypeRef tparams[count];
  count = 0;

  if(ast_id(fun) != TK_NEW)
  {
    tparams[count] = codegen_type(c, type);

    if(tparams[count] == NULL)
    {
      ast_error(fun, "couldn't generate receiver type");
      return NULL;
    }

    count++;
  }

  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* ptype = ast_childidx(param, 1);
    tparams[count] = codegen_type(c, ptype);

    if(tparams[count] == NULL)
    {
      ast_error(ptype, "couldn't generate parameter type");
      return NULL;
    }

    count++;
    param = ast_sibling(param);
  }

  ast_t* rtype = ast_childidx(fun, 4);
  LLVMTypeRef result = codegen_type(c, rtype);

  if(result == NULL)
  {
    ast_error(rtype, "couldn't generate result type");
    return NULL;
  }

  LLVMTypeRef ftype = LLVMFunctionType(result, tparams, count, false);
  func = LLVMAddFunction(c->module, funname, ftype);

  count = 0;

  if(ast_id(fun) != TK_NEW)
  {
    LLVMValueRef fparam = LLVMGetParam(func, count++);
    LLVMSetValueName(fparam, "this");
  }

  param = ast_child(params);

  while(param != NULL)
  {
    LLVMValueRef fparam = LLVMGetParam(func, count++);
    LLVMSetValueName(fparam, ast_name(ast_child(param)));
    param = ast_sibling(param);
  }

  // TODO: body
  LLVMDumpValue(func);
  return func;
}

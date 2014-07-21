#include "gencall.h"
#include "genfun.h"

static LLVMValueRef call_fun(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  int count, const char* ret)
{
  if(fun == NULL)
    return NULL;

  return LLVMBuildCall(c->builder, fun, args, count, ret);
}

LLVMValueRef codegen_call(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, LLVMValueRef* args, int count, const char* ret)
{
  return call_fun(c, codegen_function(c, type, name, typeargs), args, count,
    ret);
}

LLVMValueRef codegen_callruntime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret)
{
  return call_fun(c, LLVMGetNamedFunction(c->module, name), args, count, ret);
}

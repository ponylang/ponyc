#include "gencall.h"
#include "genfun.h"
#include "genname.h"

static LLVMValueRef call_fun(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  int count, const char* ret)
{
  if(fun == NULL)
    return NULL;

  return LLVMBuildCall(c->builder, fun, args, count, ret);
}

LLVMValueRef gencall(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, LLVMValueRef* args, int count, const char* ret)
{
  return call_fun(c, genfun(c, type, name, typeargs), args, count,
    ret);
}

LLVMValueRef gencall_runtime(compile_t* c, const char *name,
  LLVMValueRef* args, int count, const char* ret)
{
  return call_fun(c, LLVMGetNamedFunction(c->module, name), args, count, ret);
}

void gencall_tracetag(compile_t* c, LLVMValueRef field)
{
  // load the contents of the field
  LLVMValueRef field_val = LLVMBuildLoad(c->builder, field, "");

  // cast the field to a generic object pointer
  LLVMValueRef args[1];
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->object_ptr, "");

  gencall_runtime(c, "pony_trace", args, 1, "");
}

void gencall_traceclass(compile_t* c, LLVMValueRef field, const char* name)
{
  // load the contents of the field
  LLVMValueRef field_val = LLVMBuildLoad(c->builder, field, "");

  // cast the field to a generic object pointer
  LLVMValueRef args[2];
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->object_ptr, "");

  // get the trace function statically
  const char* fun = genname_fun(name, "$trace", NULL);
  args[1] = LLVMGetNamedFunction(c->module, fun);

  gencall_runtime(c, "pony_traceobject", args, 2, "");
}

void gencall_traceactor(compile_t* c, LLVMValueRef field)
{
  // load the contents of the field
  LLVMValueRef field_val = LLVMBuildLoad(c->builder, field, "");

  // cast the field to a pony_actor_t*
  LLVMValueRef args[1];
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->actor_ptr, "");

  gencall_runtime(c, "pony_traceactor", args, 1, "");
}

void gencall_traceobject(compile_t* c, LLVMValueRef field)
{
  // TODO: is this enough for all unknown types?
  // load the contents of the field
  LLVMValueRef field_val = LLVMBuildLoad(c->builder, field, "");

  // cast the field to a generic object pointer
  LLVMValueRef args[2];
  args[0] = LLVMBuildBitCast(c->builder, field_val, c->object_ptr, "");

  // get the type descriptor from the object pointer
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, args[0], 0, "");
  LLVMValueRef desc = LLVMBuildLoad(c->builder, desc_ptr, "");

  // TODO: determine if this is an actor or not

  // get the trace function from the type descriptor
  LLVMValueRef trace_ptr = LLVMBuildStructGEP(c->builder, desc, 0, "");
  args[1] = LLVMBuildLoad(c->builder, trace_ptr, "");

  gencall_runtime(c, "pony_traceobject", args, 2, "");
}

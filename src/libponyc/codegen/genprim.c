#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "gencall.h"

bool genprim_pointer(compile_t* c, gentype_t* g, bool prelim)
{
  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  gentype_t elem_g;
  bool ok;

  if(prelim)
    ok = gentype_prelim(c, typearg, &elem_g);
  else
    ok = gentype(c, typearg, &elem_g);

  if(!ok)
    return false;

  // Set the type to be a pointer to the element type.
  g->use_type = LLVMPointerType(elem_g.use_type, 0);

  // Stop here for a preliminary type.
  if(prelim)
    return true;

  // Set up a constant integer for the allocation size.
  size_t size = LLVMABISizeOfType(c->target, elem_g.use_type);
  LLVMValueRef l_size = LLVMConstInt(LLVMInt64Type(), size, false);

  // create
  const char* name = genname_fun(g->type_name, "create", NULL);
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  // If there's already a create function, we're done.
  if(fun != NULL)
    return true;

  LLVMTypeRef params[3];
  params[0] = LLVMInt64Type();

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 1, false);
  fun = LLVMAddFunction(c->module, name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef len = LLVMGetParam(fun, 0);

  LLVMValueRef args[3];
  args[0] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 1, "");
  result = LLVMBuildBitCast(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  // realloc
  name = genname_fun(g->type_name, "realloc", NULL);

  params[0] = g->use_type;
  params[1] = LLVMInt64Type();

  ftype = LLVMFunctionType(g->use_type, params, 2, false);
  fun = LLVMAddFunction(c->module, g->type_name, ftype);
  codegen_startfun(c, fun);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  args[0] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");

  len = LLVMGetParam(fun, 1);
  args[1] = LLVMBuildMul(c->builder, len, l_size, "");

  result = gencall_runtime(c, "pony_realloc", args, 2, "");
  result = LLVMBuildBitCast(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  // apply
  name = genname_fun(g->type_name, "apply", NULL);

  params[0] = g->use_type;
  params[1] = LLVMInt64Type();

  ftype = LLVMFunctionType(elem_g.use_type, params, 2, false);
  fun = LLVMAddFunction(c->module, name, ftype);
  codegen_startfun(c, fun);

  ptr = LLVMGetParam(fun, 0);
  LLVMValueRef index = LLVMGetParam(fun, 1);
  LLVMValueRef loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g.use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  // update
  name = genname_fun(g->type_name, "update", NULL);

  params[0] = g->use_type;
  params[1] = LLVMInt64Type();
  params[2] = elem_g.use_type;

  ftype = LLVMFunctionType(elem_g.use_type, params, 3, false);
  fun = LLVMAddFunction(c->module, name, ftype);
  codegen_startfun(c, fun);

  ptr = LLVMGetParam(fun, 0);
  index = LLVMGetParam(fun, 1);
  loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g.use_type, "");
  LLVMBuildStore(c->builder, LLVMGetParam(fun, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  // copy
  name = genname_fun(g->type_name, "copy", NULL);

  params[0] = g->use_type;
  params[1] = g->use_type;
  params[2] = LLVMInt64Type();

  ftype = LLVMFunctionType(LLVMInt64Type(), params, 3, false);
  fun = LLVMAddFunction(c->module, name, ftype);
  codegen_startfun(c, fun);

  args[0] = LLVMGetParam(fun, 0);
  args[1] = LLVMGetParam(fun, 1);
  args[2] = LLVMGetParam(fun, 2);
  gencall_runtime(c, "memcpy", args, 3, "");

  LLVMBuildRet(c->builder, args[2]);
  codegen_finishfun(c);

  return true;
}

void genprim_array_trace(compile_t* c, gentype_t* g)
{
  // Get the type argument for the array. This will be used to generate the
  // per-element trace call.
  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  const char* trace_name = genname_trace(g->type_name);
  LLVMValueRef trace_fn = LLVMAddFunction(c->module, trace_name, c->trace_type);
  codegen_startfun(c, trace_fn);

  LLVMValueRef arg = LLVMGetParam(trace_fn, 0);
  LLVMSetValueName(arg, "arg");

  LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(trace_fn, "cond");
  LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(trace_fn, "body");
  LLVMBasicBlockRef post_block = LLVMAppendBasicBlock(trace_fn, "post");

  // Read the count and the base pointer.
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, g->use_type, "array");
  LLVMValueRef count_ptr = LLVMBuildStructGEP(c->builder, object, 2, "");
  LLVMValueRef count = LLVMBuildLoad(c->builder, count_ptr, "count");
  LLVMValueRef pointer_ptr = LLVMBuildStructGEP(c->builder, object, 3, "");
  LLVMValueRef pointer = LLVMBuildLoad(c->builder, pointer_ptr, "pointer");

  // Trace the base pointer.
  pointer = LLVMBuildBitCast(c->builder, pointer, c->void_ptr, "");
  gencall_runtime(c, "pony_trace", &pointer, 1, "");
  LLVMBuildBr(c->builder, cond_block);

  // While the index is less than the count, trace an element. The initial
  // index when coming from the entry block is zero.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, LLVMInt64Type(), "");
  LLVMValueRef zero = LLVMConstInt(LLVMInt64Type(), 0, false);
  LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(trace_fn);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  // The phi node is the index. Get the address and trace it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef elem = LLVMBuildGEP(c->builder, pointer, &phi, 1, "elem");
  gencall_trace(c, elem, typearg);

  // Add one to the phi node and branch back to the cond block.
  LLVMValueRef one = LLVMConstInt(LLVMInt64Type(), 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildRetVoid(c->builder);

  codegen_finishfun(c);
}

void genprim_builtins(compile_t* c)
{
  // TODO: All the builtin functions.
}

#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "genfun.h"
#include "gencall.h"
#include "gentrace.h"
#include "genopt.h"
#include "../pkg/platformfuns.h"
#include "../pass/names.h"
#include "../debug/dwarf.h"
#include "../type/assemble.h"

#include <assert.h>

static void pointer_create(compile_t* c, gentype_t* g)
{
  const char* name = genname_fun(g->type_name, "create", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef result = LLVMConstNull(g->use_type);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_alloc(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  const char* name = genname_fun(g->type_name, "_alloc", NULL);
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  LLVMTypeRef params[2];
  params[0] = g->use_type;
  params[1] = c->intptr;

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 2, false);
  fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef len = LLVMGetParam(fun, 1);
  LLVMValueRef args[2];
  args[0] = codegen_ctx(c);
  args[1] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 2, "");
  result = LLVMBuildBitCast(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_realloc(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  const char* name = genname_fun(g->type_name, "_realloc", NULL);

  LLVMTypeRef params[2];
  params[0] = g->use_type;
  params[1] = c->intptr;

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 2, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef args[3];
  args[0] = codegen_ctx(c);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  args[1] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");

  LLVMValueRef len = LLVMGetParam(fun, 1);
  args[2] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_realloc", args, 3, "");
  result = LLVMBuildBitCast(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_unsafe(compile_t* c, gentype_t* g)
{
  const char* name = genname_fun(g->type_name, "_unsafe", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);
  LLVMBuildRet(c->builder, LLVMGetParam(fun, 0));
  codegen_finishfun(c);
}

static void pointer_apply(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  const char* name = genname_fun(g->type_name, "_apply", NULL);

  LLVMTypeRef params[2];
  params[0] = g->use_type;
  params[1] = c->intptr;

  LLVMTypeRef ftype = LLVMFunctionType(elem_g->use_type, params, 2, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef index = LLVMGetParam(fun, 1);
  LLVMValueRef loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_update(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  const char* name = genname_fun(g->type_name, "_update", NULL);

  LLVMTypeRef params[3];
  params[0] = g->use_type;
  params[1] = c->intptr;
  params[2] = elem_g->use_type;

  LLVMTypeRef ftype = LLVMFunctionType(elem_g->use_type, params, 3, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef index = LLVMGetParam(fun, 1);
  LLVMValueRef loc = LLVMBuildGEP(c->builder, ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g->use_type, "");
  LLVMBuildStore(c->builder, LLVMGetParam(fun, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_offset(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  const char* name = genname_fun(g->type_name, "_offset", NULL);

  LLVMTypeRef params[3];
  params[0] = g->use_type;
  params[1] = c->intptr;

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 2, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef n = LLVMGetParam(fun, 1);

  // Return ptr + (n * sizeof(len)).
  LLVMValueRef src = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef result = LLVMBuildAdd(c->builder, src, offset, "");
  result = LLVMBuildIntToPtr(c->builder, result, g->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_insert(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  const char* name = genname_fun(g->type_name, "_insert", NULL);

  LLVMTypeRef params[3];
  params[0] = g->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 3, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef n = LLVMGetParam(fun, 1);
  LLVMValueRef len = LLVMGetParam(fun, 2);

  LLVMValueRef src = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef dst = LLVMBuildAdd(c->builder, src, offset, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef args[3];
  args[0] = LLVMBuildIntToPtr(c->builder, dst, c->void_ptr, "");
  args[1] = LLVMBuildIntToPtr(c->builder, src, c->void_ptr, "");
  args[2] = elen;

  // memmove(ptr + (n * sizeof(elem)), ptr, len * sizeof(elem))
  gencall_runtime(c, "memmove", args, 3, "");

  // Return ptr.
  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);
}

static void pointer_delete(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  const char* name = genname_fun(g->type_name, "_delete", NULL);

  LLVMTypeRef params[3];
  params[0] = g->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;

  LLVMTypeRef ftype = LLVMFunctionType(elem_g->use_type, params, 3, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef n = LLVMGetParam(fun, 1);
  LLVMValueRef len = LLVMGetParam(fun, 2);

  LLVMValueRef result = LLVMBuildLoad(c->builder, ptr, "");
  result = LLVMBuildBitCast(c->builder, result, elem_g->use_type, "");

  LLVMValueRef dst = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef src = LLVMBuildAdd(c->builder, dst, offset, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef args[3];
  args[0] = LLVMBuildIntToPtr(c->builder, dst, c->void_ptr, "");
  args[1] = LLVMBuildIntToPtr(c->builder, src, c->void_ptr, "");
  args[2] = elen;

  // memmove(ptr, ptr + (n * sizeof(elem)), len * sizeof(elem))
  gencall_runtime(c, "memmove", args, 3, "");

  // Return ptr[0].
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_copy_to(compile_t* c, gentype_t* g, gentype_t* elem_g)
{
  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  const char* name = genname_fun(g->type_name, "_copy_to", NULL);

  LLVMTypeRef params[4];
  params[0] = g->use_type;
  params[1] = g->use_type;
  params[2] = c->intptr;

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 3, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef ptr2 = LLVMGetParam(fun, 1);
  LLVMValueRef n = LLVMGetParam(fun, 2);
  LLVMValueRef elen = LLVMBuildMul(c->builder, n, l_size, "");

  LLVMValueRef args[3];
  args[0] = LLVMBuildBitCast(c->builder, ptr2, c->void_ptr, "");
  args[1] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");
  args[2] = elen;

  // memcpy(ptr2, ptr, n * sizeof(elem))
  gencall_runtime(c, "memcpy", args, 3, "");

  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);
}

static void pointer_usize(compile_t* c, gentype_t* g)
{
  const char* name = genname_fun(g->type_name, "usize", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(c->intptr, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef ptr = LLVMGetParam(fun, 0);
  LLVMValueRef result = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

bool genprim_pointer(compile_t* c, gentype_t* g, bool prelim)
{
  // No trace function is generated, so the "contents" of a pointer are never
  // traced. The only exception is for the pointer held by an array, which has
  // a special trace function generated in genprim_array_trace.
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

  // If there's already a create function, we're done.
  const char* name = genname_fun(g->type_name, "create", NULL);
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  if(fun != NULL)
    return true;

  // Emit debug symbol for this pointer type instance.
  dwarf_pointer(&c->dwarf, g, elem_g.type_name);

  pointer_create(c, g);
  pointer_alloc(c, g, &elem_g);

  pointer_realloc(c, g, &elem_g);
  pointer_unsafe(c, g);
  pointer_apply(c, g, &elem_g);
  pointer_update(c, g, &elem_g);
  pointer_offset(c, g, &elem_g);
  pointer_insert(c, g, &elem_g);
  pointer_delete(c, g, &elem_g);
  pointer_copy_to(c, g, &elem_g);
  pointer_usize(c, g);

  ok = genfun_methods(c, g);

  dwarf_finish(&c->dwarf);

  return ok;
}

static void maybe_create(compile_t* c, gentype_t* g)
{
  // Returns the argument. There's no receiver.
  const char* name = genname_fun(g->type_name, "create", NULL);

  LLVMTypeRef params[2];
  params[0] = g->use_type;
  params[1] = g->structure;

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, params, 2, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef param = LLVMGetParam(fun, 1);
  LLVMValueRef result = LLVMBuildBitCast(c->builder, param, g->use_type, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void maybe_none(compile_t* c, gentype_t* g)
{
  // Returns null. There's no receiver.
  const char* name = genname_fun(g->type_name, "none", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMBuildRet(c->builder, LLVMConstNull(g->use_type));
  codegen_finishfun(c);
}

static void maybe_apply(compile_t* c, gentype_t* g)
{
  // Returns the receiver if it isn't null.
  const char* name = genname_fun(g->type_name, "apply", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(g->structure, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef result = LLVMGetParam(fun, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, result, "");

  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  LLVMPositionBuilderAtEnd(c->builder, is_false);
  result = LLVMBuildBitCast(c->builder, result, g->structure, "");
  LLVMBuildRet(c->builder, result);

  LLVMPositionBuilderAtEnd(c->builder, is_true);
  gencall_throw(c);

  codegen_finishfun(c);
}

static void maybe_is_none(compile_t* c, gentype_t* g)
{
  // Returns true if the receiver is null.
  const char* name = genname_fun(g->type_name, "is_none", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(c->i1, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef receiver = LLVMGetParam(fun, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, receiver, "");

  LLVMBuildRet(c->builder, test);
  codegen_finishfun(c);
}

bool genprim_maybe(compile_t* c, gentype_t* g, bool prelim)
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

  // Set the type to the element type.
  g->use_type = c->void_ptr;
  g->structure = elem_g.use_type;

  // Stop here for a preliminary type.
  if(prelim)
    return true;

  // If there's already a create function, we're done.
  const char* name = genname_fun(g->type_name, "create", NULL);
  LLVMValueRef fun = LLVMGetNamedFunction(c->module, name);

  if(fun != NULL)
    return true;

  // Emit debug symbol for this pointer type instance.
  dwarf_pointer(&c->dwarf, g, elem_g.type_name);

  maybe_create(c, g);
  maybe_none(c, g);
  maybe_apply(c, g);
  maybe_is_none(c, g);

  ok = genfun_methods(c, g);
  dwarf_finish(&c->dwarf);

  return ok;
}

void genprim_array_trace(compile_t* c, gentype_t* g)
{
  // Get the type argument for the array. This will be used to generate the
  // per-element trace call.
  ast_t* typeargs = ast_childidx(g->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  const char* trace_name = genname_trace(g->type_name);
  LLVMValueRef trace_fn = codegen_addfun(c, trace_name, c->trace_type);

  codegen_startfun(c, trace_fn, false);
  LLVMSetFunctionCallConv(trace_fn, LLVMCCallConv);
  LLVMValueRef ctx = LLVMGetParam(trace_fn, 0);
  LLVMValueRef arg = LLVMGetParam(trace_fn, 1);

  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  // Read the count and the base pointer.
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, g->use_type,
    "array");
  LLVMValueRef count_ptr = LLVMBuildStructGEP(c->builder, object, 1, "");
  LLVMValueRef count = LLVMBuildLoad(c->builder, count_ptr, "count");
  LLVMValueRef pointer_ptr = LLVMBuildStructGEP(c->builder, object, 3, "");
  LLVMValueRef pointer = LLVMBuildLoad(c->builder, pointer_ptr, "pointer");

  // Trace the base pointer.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, pointer, c->void_ptr, "");
  gencall_runtime(c, "pony_trace", args, 2, "");
  LLVMValueRef branch = LLVMBuildBr(c->builder, cond_block);

  // While the index is less than the count, trace an element. The initial
  // index when coming from the entry block is zero.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");
  LLVMValueRef zero = LLVMConstInt(c->intptr, 0, false);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  // The phi node is the index. Get the element and trace it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef elem_ptr = LLVMBuildGEP(c->builder, pointer, &phi, 1, "elem");
  LLVMValueRef elem = LLVMBuildLoad(c->builder, elem_ptr, "");

  if(gentrace(c, ctx, elem, typearg))
  {
    // Add one to the phi node and branch back to the cond block.
    LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
    LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
    body_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &inc, &body_block, 1);
    LLVMBuildBr(c->builder, cond_block);

    LLVMPositionBuilderAtEnd(c->builder, post_block);
  } else {
    // No tracing is required.
    LLVMInstructionEraseFromParent(branch);
    LLVMInstructionEraseFromParent(elem);
    LLVMInstructionEraseFromParent(elem_ptr);

    LLVMDeleteBasicBlock(cond_block);
    LLVMDeleteBasicBlock(body_block);
    LLVMDeleteBasicBlock(post_block);
    LLVMPositionBuilderAtEnd(c->builder, entry_block);
  }


  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

typedef struct num_conv_t
{
  const char* type_name;
  const char* fun_name;
  LLVMTypeRef type;
  int size;
  bool is_signed;
  bool is_float;
} num_conv_t;

static void number_conversions(compile_t* c)
{
  bool ilp32 = target_is_ilp32(c->opt->triple);
  bool llp64 = target_is_llp64(c->opt->triple);
  bool lp64 = target_is_lp64(c->opt->triple);


  num_conv_t* conv;

  num_conv_t ilp32_conv[] =
  {
    {"I8", "i8", c->i8, 8, true, false},
    {"I16", "i16", c->i16, 16, true, false},
    {"I32", "i32", c->i32, 32, true, false},
    {"I64", "i64", c->i64, 64, true, false},

    {"U8", "u8", c->i8, 8, false, false},
    {"U16", "u16", c->i16, 16, false, false},
    {"U32", "u32", c->i32, 32, false, false},
    {"U64", "u64", c->i64, 64, false, false},
    {"I128", "i128", c->i128, 128, true, false},
    {"U128", "u128", c->i128, 128, false, false},

    {"ILong", "ilong", c->i32, 32, true, false},
    {"ULong", "ulong", c->i32, 32, false, false},
    {"ISize", "isize", c->i32, 32, true, false},
    {"USize", "usize", c->i32, 32, false, false},

    {"F32", "f32", c->f32, 32, false, true},
    {"F64", "f64", c->f64, 64, false, true},

    {NULL, NULL, NULL, false, false, false}
  };

  num_conv_t lp64_conv[] =
  {
    {"I8", "i8", c->i8, 8, true, false},
    {"I16", "i16", c->i16, 16, true, false},
    {"I32", "i32", c->i32, 32, true, false},
    {"I64", "i64", c->i64, 64, true, false},

    {"U8", "u8", c->i8, 8, false, false},
    {"U16", "u16", c->i16, 16, false, false},
    {"U32", "u32", c->i32, 32, false, false},
    {"U64", "u64", c->i64, 64, false, false},
    {"I128", "i128", c->i128, 128, true, false},
    {"U128", "u128", c->i128, 128, false, false},

    {"ILong", "ilong", c->i64, 64, true, false},
    {"ULong", "ulong", c->i64, 64, false, false},
    {"ISize", "isize", c->i64, 64, true, false},
    {"USize", "usize", c->i64, 64, false, false},

    {"F32", "f32", c->f32, 32, false, true},
    {"F64", "f64", c->f64, 64, false, true},

    {NULL, NULL, NULL, false, false, false}
  };

  num_conv_t llp64_conv[] =
  {
    {"I8", "i8", c->i8, 8, true, false},
    {"I16", "i16", c->i16, 16, true, false},
    {"I32", "i32", c->i32, 32, true, false},
    {"I64", "i64", c->i64, 64, true, false},

    {"U8", "u8", c->i8, 8, false, false},
    {"U16", "u16", c->i16, 16, false, false},
    {"U32", "u32", c->i32, 32, false, false},
    {"U64", "u64", c->i64, 64, false, false},
    {"I128", "i128", c->i128, 128, true, false},
    {"U128", "u128", c->i128, 128, false, false},

    {"ILong", "ilong", c->i32, 32, true, false},
    {"ULong", "ulong", c->i32, 32, false, false},
    {"ISize", "isize", c->i64, 64, true, false},
    {"USize", "usize", c->i64, 64, false, false},

    {"F32", "f32", c->f32, 32, false, true},
    {"F64", "f64", c->f64, 64, false, true},

    {NULL, NULL, NULL, false, false, false}
  };

  if(ilp32)
    conv = ilp32_conv;

  if(lp64)
    conv = lp64_conv;

  if(llp64)
    conv = llp64_conv;


  assert(conv != NULL);

  bool native128 = target_is_native128(c->opt->triple);

  for(num_conv_t* from = conv; from->type_name != NULL; from++)
  {
    for(num_conv_t* to = conv; to->type_name != NULL; to++)
    {
      if(to->fun_name == NULL)
        continue;

      const char* name = genname_fun(from->type_name, to->fun_name, NULL);
      LLVMTypeRef f_type = LLVMFunctionType(to->type, &from->type, 1, false);
      LLVMValueRef fun = codegen_addfun(c, name, f_type);

      codegen_startfun(c, fun, false);
      LLVMValueRef arg = LLVMGetParam(fun, 0);
      LLVMValueRef result;

      if(from->is_float)
      {
        if(to->is_float)
        {
          if(from->size < to->size)
            result = LLVMBuildFPExt(c->builder, arg, to->type, "");
          else if(from->size > to->size)
            result = LLVMBuildFPTrunc(c->builder, arg, to->type, "");
          else
            result = arg;
        } else if(to->is_signed) {
          result = LLVMBuildFPToSI(c->builder, arg, to->type, "");
        } else {
          result = LLVMBuildFPToUI(c->builder, arg, to->type, "");
        }
      } else if(to->is_float) {
        if(from->is_signed)
          result = LLVMBuildSIToFP(c->builder, arg, to->type, "");
        else
          result = LLVMBuildUIToFP(c->builder, arg, to->type, "");
      } else if(from->size > to->size) {
          result = LLVMBuildTrunc(c->builder, arg, to->type, "");
      } else if(from->size < to->size) {
        if(from->is_signed)
          result = LLVMBuildSExt(c->builder, arg, to->type, "");
        else
          result = LLVMBuildZExt(c->builder, arg, to->type, "");
      } else {
        result = arg;
      }

      LLVMBuildRet(c->builder, result);
      codegen_finishfun(c);

      if(!native128 &&
        ((from->is_float && (to->size > 64)) ||
        (to->is_float && (from->size > 64)))
        )
      {
        LLVMDeleteFunction(fun);
      }
    }
  }
}

static void fp_as_bits(compile_t* c)
{
  const char* name;
  LLVMTypeRef f_type;
  LLVMTypeRef params[2];
  LLVMValueRef fun, result;

  name = genname_fun("F32", "from_bits", NULL);
  params[0] = c->f32;
  params[1] = c->i32;
  f_type = LLVMFunctionType(c->f32, params, 2, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 1), c->f32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("F32", "bits", NULL);
  f_type = LLVMFunctionType(c->i32, &c->f32, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->i32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("F64", "from_bits", NULL);
  params[0] = c->f64;
  params[1] = c->i64;
  f_type = LLVMFunctionType(c->f64, params, 2, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 1), c->f64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  name = genname_fun("F64", "bits", NULL);
  f_type = LLVMFunctionType(c->i64, &c->f64, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->i64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void make_cpuid(compile_t* c)
{
  if(target_is_x86(c->opt->triple))
  {
    LLVMTypeRef elems[4] = {c->i32, c->i32, c->i32, c->i32};
    LLVMTypeRef r_type = LLVMStructTypeInContext(c->context, elems, 4, false);
    LLVMTypeRef f_type = LLVMFunctionType(r_type, &c->i32, 1, false);
    LLVMValueRef fun = codegen_addfun(c, "internal.x86.cpuid", f_type);
    LLVMSetFunctionCallConv(fun, LLVMCCallConv);
    codegen_startfun(c, fun, false);

    LLVMValueRef cpuid = LLVMConstInlineAsm(f_type,
      "cpuid", "={ax},={bx},={cx},={dx},{ax}", false, false);
    LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);

    LLVMValueRef result = LLVMBuildCall(c->builder, cpuid, &zero, 1, "");
    LLVMBuildRet(c->builder, result);

    codegen_finishfun(c);
  } else {
    (void)c;
  }
}

static void make_rdtscp(compile_t* c)
{
  if(target_is_x86(c->opt->triple))
  {
    // i64 @llvm.x86.rdtscp(i8*)
    LLVMTypeRef f_type = LLVMFunctionType(c->i64, &c->void_ptr, 1, false);
    LLVMValueRef rdtscp = LLVMAddFunction(c->module, "llvm.x86.rdtscp", f_type);

    // i64 @internal.x86.rdtscp(i32*)
    LLVMTypeRef i32_ptr = LLVMPointerType(c->i32, 0);
    f_type = LLVMFunctionType(c->i64, &i32_ptr, 1, false);
    LLVMValueRef fun = codegen_addfun(c, "internal.x86.rdtscp", f_type);
    LLVMSetFunctionCallConv(fun, LLVMCCallConv);
    codegen_startfun(c, fun, false);

    // Cast i32* to i8* and call the intrinsic.
    LLVMValueRef arg = LLVMGetParam(fun, 0);
    arg = LLVMBuildBitCast(c->builder, arg, c->void_ptr, "");
    LLVMValueRef result = LLVMBuildCall(c->builder, rdtscp, &arg, 1, "");
    LLVMBuildRet(c->builder, result);

    codegen_finishfun(c);
  } else {
    (void)c;
  }
}

void genprim_builtins(compile_t* c)
{
  number_conversions(c);
  fp_as_bits(c);
  make_cpuid(c);
  make_rdtscp(c);
}

void genprim_reachable_init(compile_t* c, ast_t* program)
{
  // Look for primitives in all packages that have _init or _final methods.
  // Mark them as reachable.
  const char* init = stringtab("_init");
  const char* final = stringtab("_final");
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    ast_t* module = ast_child(package);

    while(module != NULL)
    {
      ast_t* entity = ast_child(module);

      while(entity != NULL)
      {
        if(ast_id(entity) == TK_PRIMITIVE)
        {
          AST_GET_CHILDREN(entity, id, typeparams);

          if(ast_id(typeparams) == TK_NONE)
          {
            ast_t* type = type_builtin(c->opt, entity, ast_name(id));
            ast_t* finit = ast_get(entity, init, NULL);
            ast_t* ffinal = ast_get(entity, final, NULL);

            if(finit != NULL)
            {
              reach(c->reachable, &c->next_type_id, type, init, NULL);
              ast_free_unattached(finit);
            }

            if(ffinal != NULL)
            {
              reach(c->reachable, &c->next_type_id, type, final, NULL);
              ast_free_unattached(ffinal);
            }

            ast_free_unattached(type);
          }
        }

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }

    package = ast_sibling(package);
  }
}

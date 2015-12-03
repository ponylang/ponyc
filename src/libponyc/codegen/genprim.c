#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "genfun.h"
#include "gencall.h"
#include "gentrace.h"
#include "../pkg/platformfuns.h"
#include "../pass/names.h"
#include "../debug/dwarf.h"
#include "../type/assemble.h"

static void pointer_create(compile_t* c, gentype_t* g)
{
  const char* name = genname_fun(g->type_name, "create", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, NULL, 0, false);
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

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, &c->intptr, 1, false);
  fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef len = LLVMGetParam(fun, 0);
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

static void pointer_offset(compile_t* c, gentype_t* g, gentype_t* elem_g,
  const char* name)
{
  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, elem_g->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  name = genname_fun(g->type_name, name, NULL);

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
  pointer_apply(c, g, &elem_g);
  pointer_update(c, g, &elem_g);
  pointer_offset(c, g, &elem_g, "_offset");
  pointer_offset(c, g, &elem_g, "_offset_tag");
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

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMBuildRet(c->builder, LLVMGetParam(fun, 0));
  codegen_finishfun(c);
}

static void maybe_none(compile_t* c, gentype_t* g)
{
  // Returns null. There's no receiver.
  const char* name = genname_fun(g->type_name, "none", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, NULL, 0, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMBuildRet(c->builder, LLVMConstNull(g->use_type));
  codegen_finishfun(c);
}

static void maybe_apply(compile_t* c, gentype_t* g)
{
  ast_t* type_args = ast_childidx(g->ast, 2);
  ast_t* elem = ast_child(type_args);

  // Returns the receiver if it isn't null.
  const char* name = genname_fun(g->type_name, "apply", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(g->use_type, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef result = LLVMGetParam(fun, 0);
  LLVMValueRef test = genprim_maybe_is_null(c, elem, result);

  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  LLVMPositionBuilderAtEnd(c->builder, is_false);
  LLVMBuildRet(c->builder, result);

  LLVMPositionBuilderAtEnd(c->builder, is_true);
  gencall_throw(c);

  codegen_finishfun(c);
}

static void maybe_is_none(compile_t* c, gentype_t* g)
{
  ast_t* type_args = ast_childidx(g->ast, 2);
  ast_t* elem = ast_child(type_args);

  // Returns true if the receiver is null.
  const char* name = genname_fun(g->type_name, "is_none", NULL);

  LLVMTypeRef ftype = LLVMFunctionType(c->i1, &g->use_type, 1, false);
  LLVMValueRef fun = codegen_addfun(c, name, ftype);
  codegen_startfun(c, fun, false);

  LLVMValueRef receiver = LLVMGetParam(fun, 0);
  LLVMValueRef test = genprim_maybe_is_null(c, elem, receiver);

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
  g->use_type = elem_g.use_type;

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

LLVMValueRef genprim_maybe_is_null(compile_t* c, ast_t* type,
  LLVMValueRef value)
{
  LLVMValueRef test;

  if(ast_id(type) != TK_TUPLETYPE)
  {
    test = LLVMBuildIsNull(c->builder, value, "");
  } else {
    test = LLVMConstInt(c->i1, 1, false);
    ast_t* child = ast_child(type);
    int i = 0;

    while(child != NULL)
    {
      LLVMValueRef child_value = LLVMBuildExtractValue(c->builder, value,
        i, "");
      LLVMValueRef child_test = genprim_maybe_is_null(c, child, child_value);
      test = LLVMBuildAnd(c->builder, test, child_test, "");

      child = ast_sibling(child);
      i++;
    }
  }

  return test;
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
  LLVMBuildBr(c->builder, cond_block);

  // While the index is less than the count, trace an element. The initial
  // index when coming from the entry block is zero.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");
  LLVMValueRef zero = LLVMConstInt(c->intptr, 0, false);
  LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(trace_fn);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  // The phi node is the index. Get the element and trace it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef elem = LLVMBuildGEP(c->builder, pointer, &phi, 1, "elem");
  elem = LLVMBuildLoad(c->builder, elem, "");
  gentrace(c, ctx, elem, typearg);

  // Add one to the phi node and branch back to the cond block.
  LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  body_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
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
  num_conv_t conv[] =
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

#if defined(PLATFORM_IS_ILP32)
    {"ILong", "ilong", c->i32, 32, true, false},
    {"ULong", "ulong", c->i32, 32, false, false},
    {"ISize", "isize", c->i32, 32, true, false},
    {"USize", "usize", c->i32, 32, false, false},
#elif defined(PLATFORM_IS_LP64)
    {"ILong", "ilong", c->i64, 64, true, false},
    {"ULong", "ulong", c->i64, 64, false, false},
    {"ISize", "isize", c->i64, 64, true, false},
    {"USize", "usize", c->i64, 64, false, false},
#elif defined(PLATFORM_IS_LLP64)
    {"ILong", "ilong", c->i32, 32, true, false},
    {"ULong", "ulong", c->i32, 32, false, false},
    {"ISize", "isize", c->i64, 64, true, false},
    {"USize", "usize", c->i64, 64, false, false},
#endif

    {"F32", "f32", c->f32, 32, false, true},
    {"F64", "f64", c->f64, 64, false, true},

    {NULL, NULL, NULL, false, false, false}
  };

  bool native128;
  os_is_target(OS_NATIVE128_NAME, c->opt->release, &native128);

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

typedef struct num_cons_t
{
  const char* type_name;
  LLVMTypeRef type;
} num_cons_t;

static void number_constructors(compile_t* c)
{
  num_cons_t cons[] =
  {
    {"Bool", c->i1},

    {"I8", c->i8},
    {"I16", c->i16},
    {"I32", c->i32},
    {"I64", c->i64},

    {"U8", c->i8},
    {"U16", c->i16},
    {"U32", c->i32},
    {"U64", c->i64},

#if defined(PLATFORM_IS_ILP32)
    {"ILong", c->i32},
    {"ULong", c->i32},

    {"ISize", c->i32},
    {"USize", c->i32},
#elif defined(PLATFORM_IS_LP64)
    {"ILong", c->i64},
    {"ULong", c->i64},

    {"ISize", c->i64},
    {"USize", c->i64},

    {"I128", c->i128},
    {"U128", c->i128},
#elif defined(PLATFORM_IS_LLP64)
    {"ILong", c->i32},
    {"ULong", c->i32},

    {"ISize", c->i64},
    {"USize", c->i64},

    {"I128", c->i128},
    {"U128", c->i128},
#endif

    {"F32", c->f32},
    {"F64", c->f64},

    {NULL, NULL}
  };

  for(num_cons_t* con = cons; con->type_name != NULL; con++)
  {
    const char* name = genname_fun(con->type_name, "create", NULL);
    LLVMTypeRef f_type = LLVMFunctionType(con->type, &con->type, 1, false);
    LLVMValueRef fun = codegen_addfun(c, name, f_type);

    codegen_startfun(c, fun, false);
    LLVMValueRef arg = LLVMGetParam(fun, 0);
    LLVMBuildRet(c->builder, arg);
    codegen_finishfun(c);
  }
}

static void special_number_constructors(compile_t* c)
{
  const char* name = genname_fun("F32", "pi", NULL);
  LLVMTypeRef f_type = LLVMFunctionType(c->f32, NULL, 0, false);
  LLVMValueRef fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f32, 3.14159265358979323846));
  codegen_finishfun(c);

  name = genname_fun("F32", "e", NULL);
  f_type = LLVMFunctionType(c->f32, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f32, 2.71828182845904523536));
  codegen_finishfun(c);

  name = genname_fun("F64", "pi", NULL);
  f_type = LLVMFunctionType(c->f64, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f64, 3.14159265358979323846));
  codegen_finishfun(c);

  name = genname_fun("F64", "e", NULL);
  f_type = LLVMFunctionType(c->f64, NULL, 0, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  LLVMBuildRet(c->builder, LLVMConstReal(c->f64, 2.71828182845904523536));
  codegen_finishfun(c);
}

static void fp_as_bits(compile_t* c)
{
  const char* name;
  LLVMTypeRef f_type;
  LLVMValueRef fun, result;

  name = genname_fun("F32", "from_bits", NULL);
  f_type = LLVMFunctionType(c->f32, &c->i32, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->f32, "");
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
  f_type = LLVMFunctionType(c->f64, &c->i64, 1, false);
  fun = codegen_addfun(c, name, f_type);

  codegen_startfun(c, fun, false);
  result = LLVMBuildBitCast(c->builder, LLVMGetParam(fun, 0), c->f64, "");
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
#ifdef PLATFORM_IS_X86
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
#else
  (void)c;
#endif
}

static void make_rdtscp(compile_t* c)
{
#ifdef PLATFORM_IS_X86
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
#else
  (void)c;
#endif
}

void genprim_builtins(compile_t* c)
{
  number_conversions(c);
  number_constructors(c);
  special_number_constructors(c);
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
          ast_t* id = ast_child(entity);
          ast_t* type = type_builtin(c->opt, entity, ast_name(id));
          ast_t* finit = ast_get(entity, init, NULL);
          ast_t* ffinal = ast_get(entity, final, NULL);

          if(finit != NULL)
          {
            reach(c->reachable, type, init, NULL);
            ast_free_unattached(finit);
          }

          if(ffinal != NULL)
          {
            reach(c->reachable, type, final, NULL);
            ast_free_unattached(ffinal);
          }

          ast_free_unattached(type);
        }

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }

    package = ast_sibling(package);
  }
}

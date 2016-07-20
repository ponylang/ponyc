#include "genprim.h"
#include "genname.h"
#include "genfun.h"
#include "gencall.h"
#include "gentrace.h"
#include "genopt.h"
#include "genserialise.h"
#include "../pkg/platformfuns.h"
#include "../pass/names.h"
#include "../type/assemble.h"

#include <assert.h>
#include <string.h>

#define FIND_METHOD(name) \
  const char* strtab_name = stringtab(name); \
  reach_method_t* m = reach_method(t, TK_NONE, strtab_name, NULL); \
  if(m == NULL) return; \
  m->intrinsic = true;

#define BOX_FUNCTION() \
  box_function(t, m, strtab_name);

static void start_function(compile_t* c, reach_method_t* m,
  LLVMTypeRef result, LLVMTypeRef* params, unsigned count)
{
  m->func_type = LLVMFunctionType(result, params, count, false);
  m->func = codegen_addfun(c, m->full_name, m->func_type);
  codegen_startfun(c, m->func, NULL, NULL);
}

static void box_function(reach_type_t* t, reach_method_t* m, const char* name)
{
  if((m->cap != TK_BOX) && (m->cap != TK_TAG))
    return;

  reach_method_t* m_ref = reach_method(t, TK_REF, name, NULL);

  if(m_ref != NULL)
  {
    m_ref->func_type = m->func_type;
    m_ref->func = m->func;
    m_ref->intrinsic = true;
  }

  reach_method_t* m_val = reach_method(t, TK_VAL, name, NULL);

  if(m_val != NULL)
  {
    m_val->func_type = m->func_type;
    m_val->func = m->func;
    m_val->intrinsic = true;
  }

  if(m->cap == TK_TAG)
  {
    reach_method_t* m_box = reach_method(t, TK_BOX, name, NULL);

    if(m_box != NULL)
    {
      m_box->func_type = m->func_type;
      m_box->func = m->func;
      m_box->intrinsic = true;
    }
  }
}

static LLVMValueRef field_loc(compile_t* c, LLVMValueRef offset,
  LLVMTypeRef structure, LLVMTypeRef ftype, int index)
{
  LLVMValueRef f_offset = LLVMBuildAdd(c->builder, offset,
    LLVMConstInt(c->intptr,
      LLVMOffsetOfElement(c->target_data, structure, index), false), "");

  return LLVMBuildIntToPtr(c->builder, f_offset,
    LLVMPointerType(ftype, 0), "");
}

static LLVMValueRef field_value(compile_t* c, LLVMValueRef object, int index)
{
  LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, index, "");
  return LLVMBuildLoad(c->builder, field, "");
}

static void pointer_create(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("create");
  start_function(c, m, t->use_type, &t->use_type, 1);

  LLVMValueRef result = LLVMConstNull(t->use_type);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_alloc(compile_t* c, reach_type_t* t,
  reach_type_t* t_elem)
{
  FIND_METHOD("_alloc");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t->use_type, params, 2);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMSetReturnNoAlias(m->func);
#if PONY_LLVM >= 307
  LLVMSetDereferenceableOrNull(m->func, 0, size);
#endif

  LLVMValueRef len = LLVMGetParam(m->func, 1);
  LLVMValueRef args[2];
  args[0] = codegen_ctx(c);
  args[1] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 2, "");
  result = LLVMBuildBitCast(c->builder, result, t->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_realloc(compile_t* c, reach_type_t* t,
  reach_type_t* t_elem)
{
  FIND_METHOD("_realloc");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t->use_type, params, 2);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMSetReturnNoAlias(m->func);
#if PONY_LLVM >= 307
  LLVMSetDereferenceableOrNull(m->func, 0, size);
#endif

  LLVMValueRef args[3];
  args[0] = codegen_ctx(c);
  args[1] = LLVMGetParam(m->func, 0);

  LLVMValueRef len = LLVMGetParam(m->func, 1);
  args[2] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_realloc", args, 3, "");
  result = LLVMBuildBitCast(c->builder, result, t->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_unsafe(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_unsafe");
  start_function(c, m, t->use_type, &t->use_type, 1);

  LLVMBuildRet(c->builder, LLVMGetParam(m->func, 0));
  codegen_finishfun(c);
}

static void pointer_apply(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("_apply");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t_elem->use_type, params, 2);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef index = LLVMGetParam(m->func, 1);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef loc = LLVMBuildGEP(c->builder, elem_ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void pointer_update(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("_update");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = t_elem->use_type;
  start_function(c, m, t_elem->use_type, params, 3);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef index = LLVMGetParam(m->func, 1);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef loc = LLVMBuildGEP(c->builder, elem_ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");
  LLVMBuildStore(c->builder, LLVMGetParam(m->func, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_offset(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("_offset");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t->use_type, params, 2);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);

  // Return ptr + (n * sizeof(len)).
  LLVMValueRef src = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef result = LLVMBuildAdd(c->builder, src, offset, "");
  result = LLVMBuildIntToPtr(c->builder, result, t->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void pointer_insert(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("_insert");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, m, t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);
  LLVMValueRef len = LLVMGetParam(m->func, 2);

  LLVMValueRef src = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef dst = LLVMBuildAdd(c->builder, src, offset, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef args[5];
  args[0] = LLVMBuildIntToPtr(c->builder, dst, c->void_ptr, "");
  args[1] = LLVMBuildIntToPtr(c->builder, src, c->void_ptr, "");
  args[2] = elen;
  args[3] = LLVMConstInt(c->i32, 1, false);
  args[4] = LLVMConstInt(c->i1, 0, false);

  // llvm.memmove.*(ptr + (n * sizeof(elem)), ptr, len * sizeof(elem))
  if(target_is_ilp32(c->opt->triple))
  {
    gencall_runtime(c, "llvm.memmove.p0i8.p0i8.i32", args, 5, "");
  } else {
    gencall_runtime(c, "llvm.memmove.p0i8.p0i8.i64", args, 5, "");
  }

  // Return ptr.
  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);
}

static void pointer_delete(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("_delete");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, m, t_elem->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);
  LLVMValueRef len = LLVMGetParam(m->func, 2);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, elem_ptr, "");

  LLVMValueRef dst = LLVMBuildPtrToInt(c->builder, elem_ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef src = LLVMBuildAdd(c->builder, dst, offset, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef args[5];
  args[0] = LLVMBuildIntToPtr(c->builder, dst, c->void_ptr, "");
  args[1] = LLVMBuildIntToPtr(c->builder, src, c->void_ptr, "");
  args[2] = elen;
  args[3] = LLVMConstInt(c->i32, 1, false);
  args[4] = LLVMConstInt(c->i1, 0, false);

  // llvm.memmove.*(ptr, ptr + (n * sizeof(elem)), len * sizeof(elem))
  if(target_is_ilp32(c->opt->triple))
  {
    gencall_runtime(c, "llvm.memmove.p0i8.p0i8.i32", args, 5, "");
  } else {
    gencall_runtime(c, "llvm.memmove.p0i8.p0i8.i64", args, 5, "");
  }

  // Return ptr[0].
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_copy_to(compile_t* c, reach_type_t* t,
  reach_type_t* t_elem)
{
  FIND_METHOD("_copy_to");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = t->use_type;
  params[2] = c->intptr;
  start_function(c, m, t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(m->func, 1);
  LLVMValueRef n = LLVMGetParam(m->func, 2);
  LLVMValueRef elen = LLVMBuildMul(c->builder, n, l_size, "");

  LLVMValueRef args[5];
  args[0] = LLVMBuildBitCast(c->builder, ptr2, c->void_ptr, "");
  args[1] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");
  args[2] = elen;
  args[3] = LLVMConstInt(c->i32, 1, false);
  args[4] = LLVMConstInt(c->i1, 0, false);

  // llvm.memcpy.*(ptr2, ptr, n * sizeof(elem), 1, 0)
  if(target_is_ilp32(c->opt->triple))
  {
    gencall_runtime(c, "llvm.memcpy.p0i8.p0i8.i32", args, 5, "");
  } else {
    gencall_runtime(c, "llvm.memcpy.p0i8.p0i8.i64", args, 5, "");
  }

  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void pointer_usize(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("usize");
  start_function(c, m, c->intptr, &t->use_type, 1);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef result = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

void genprim_pointer_methods(compile_t* c, reach_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);

  pointer_create(c, t);
  pointer_alloc(c, t, t_elem);

  pointer_realloc(c, t, t_elem);
  pointer_unsafe(c, t);
  pointer_apply(c, t, t_elem);
  pointer_update(c, t, t_elem);
  pointer_offset(c, t, t_elem);
  pointer_insert(c, t, t_elem);
  pointer_delete(c, t, t_elem);
  pointer_copy_to(c, t, t_elem);
  pointer_usize(c, t);
}

static void maybe_create(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("create");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = t_elem->use_type;
  start_function(c, m, t->use_type, params, 2);

  LLVMValueRef param = LLVMGetParam(m->func, 1);
  LLVMValueRef result = LLVMBuildBitCast(c->builder, param, t->use_type, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void maybe_none(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("none");
  start_function(c, m, t->use_type, &t->use_type, 1);

  LLVMBuildRet(c->builder, LLVMConstNull(t->use_type));
  codegen_finishfun(c);
}

static void maybe_apply(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  // Returns the receiver if it isn't null.
  FIND_METHOD("apply");
  start_function(c, m, t_elem->use_type, &t->use_type, 1);

  LLVMValueRef result = LLVMGetParam(m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, result, "");

  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  LLVMPositionBuilderAtEnd(c->builder, is_false);
  result = LLVMBuildBitCast(c->builder, result, t_elem->use_type, "");
  LLVMBuildRet(c->builder, result);

  LLVMPositionBuilderAtEnd(c->builder, is_true);
  gencall_throw(c);

  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void maybe_is_none(compile_t* c, reach_type_t* t)
{
  // Returns true if the receiver is null.
  FIND_METHOD("is_none");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef receiver = LLVMGetParam(m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, receiver, "");
  LLVMValueRef value = LLVMBuildZExt(c->builder, test, c->ibool, "");

  LLVMBuildRet(c->builder, value);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

void genprim_maybe_methods(compile_t* c, reach_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);

  maybe_create(c, t, t_elem);
  maybe_none(c, t);
  maybe_apply(c, t, t_elem);
  maybe_is_none(c, t);
}

static void donotoptimise_apply(compile_t* c, reach_type_t* t,
  reach_method_t* m)
{
  const char* strtab_name = m->name;
  m->intrinsic = true;

  ast_t* typearg = ast_child(m->typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);
  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = t_elem->use_type;

  start_function(c, m, m->result->use_type, params, 2);

  LLVMValueRef obj = LLVMGetParam(m->func, 1);
  LLVMTypeRef void_fn = LLVMFunctionType(c->void_type, &t_elem->use_type, 1,
    false);
  LLVMValueRef asmstr = LLVMConstInlineAsm(void_fn, "", "imr,~{memory}", true,
    false);
  LLVMBuildCall(c->builder, asmstr, &obj, 1, "");

  LLVMBuildRet(c->builder, m->result->instance);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void donotoptimise_observe(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("observe");

  start_function(c, m, m->result->use_type, &t->use_type, 1);

  LLVMTypeRef void_fn = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMValueRef asmstr = LLVMConstInlineAsm(void_fn, "", "~{memory}", true,
    false);
  LLVMBuildCall(c->builder, asmstr, NULL, 0, "");

  LLVMBuildRet(c->builder, m->result->instance);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

void genprim_donotoptimise_methods(compile_t* c, reach_type_t* t)
{
  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* mn;

  const char* str_apply = stringtab("apply");
  while((mn = reach_method_names_next(&t->methods, &i)) != NULL)
  {
    if(mn->name == str_apply)
    {
      size_t j = HASHMAP_BEGIN;
      reach_method_t* m;
      while((m = reach_methods_next(&mn->r_methods, &j)) != NULL)
        donotoptimise_apply(c, t, m);
      break;
    }
  }
  donotoptimise_observe(c, t);
}

static void trace_array_elements(compile_t* c, reach_type_t* t,
  LLVMValueRef ctx, LLVMValueRef object, LLVMValueRef pointer)
{
  // Get the type argument for the array. This will be used to generate the
  // per-element trace call.
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  if(!gentrace_needed(typearg))
    return;

  reach_type_t* t_elem = reach_type(c->reach, typearg);
  pointer = LLVMBuildBitCast(c->builder, pointer,
    LLVMPointerType(t_elem->use_type, 0), "");

  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  // Read the size.
  LLVMValueRef size = field_value(c, object, 1);
  LLVMBuildBr(c->builder, cond_block);

  // While the index is less than the size, trace an element. The initial
  // index when coming from the entry block is zero.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");
  LLVMValueRef zero = LLVMConstInt(c->intptr, 0, false);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, size, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  // The phi node is the index. Get the element and trace it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef elem_ptr = LLVMBuildGEP(c->builder, pointer, &phi, 1, "elem");
  LLVMValueRef elem = LLVMBuildLoad(c->builder, elem_ptr, "");
  gentrace(c, ctx, elem, typearg);

  // Add one to the phi node and branch back to the cond block.
  LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  body_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
}

void genprim_array_trace(compile_t* c, reach_type_t* t)
{
  codegen_startfun(c, t->trace_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->trace_fn, LLVMCCallConv);
  LLVMSetLinkage(t->trace_fn, LLVMExternalLinkage);
  LLVMValueRef ctx = LLVMGetParam(t->trace_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->trace_fn, 1);

  // Read the base pointer.
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->use_type, "");
  LLVMValueRef pointer = field_value(c, object, 3);

  // Trace the base pointer.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = pointer;
  gencall_runtime(c, "pony_trace", args, 2, "");

  trace_array_elements(c, t, ctx, object, pointer);
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

void genprim_array_serialise_trace(compile_t* c, reach_type_t* t)
{
  // Generate the serialise_trace function.
  t->serialise_trace_fn = codegen_addfun(c, genname_serialise_trace(t->name),
    c->trace_type);

  codegen_startfun(c, t->serialise_trace_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->serialise_trace_fn, LLVMCCallConv);
  LLVMSetLinkage(t->serialise_trace_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->serialise_trace_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->serialise_trace_fn, 1);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->use_type, "");

  // Read the size.
  LLVMValueRef size = field_value(c, object, 1);

  // Calculate the size of the element type.
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);

  size_t abisize = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, abisize, false);

  // Reserve space for the array elements.
  LLVMValueRef pointer = field_value(c, object, 3);

  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = pointer;
  args[2] = LLVMBuildMul(c->builder, size, l_size, "");
  gencall_runtime(c, "pony_serialise_reserve", args, 3, "");

  // Trace the array elements.
  trace_array_elements(c, t, ctx, object, pointer);

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

void genprim_array_serialise(compile_t* c, reach_type_t* t)
{
  // Generate the serialise function.
  t->serialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->serialise_type);

  codegen_startfun(c, t->serialise_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->serialise_fn, LLVMCCallConv);
  LLVMSetLinkage(t->serialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->serialise_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->serialise_fn, 1);
  LLVMValueRef addr = LLVMGetParam(t->serialise_fn, 2);
  LLVMValueRef offset = LLVMGetParam(t->serialise_fn, 3);
  LLVMValueRef mut = LLVMGetParam(t->serialise_fn, 4);

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->structure_ptr,
    "");
  LLVMValueRef offset_addr = LLVMBuildAdd(c->builder,
    LLVMBuildPtrToInt(c->builder, addr, c->intptr, ""), offset, "");

  genserialise_typeid(c, t, offset_addr);

  // Don't serialise our contents if we are opaque.
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntNE, mut,
    LLVMConstInt(c->i32, PONY_TRACE_OPAQUE, false), "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);
  LLVMPositionBuilderAtEnd(c->builder, body_block);

  // Write the size twice, effectively rewriting alloc to be the same as size.
  LLVMValueRef size = field_value(c, object, 1);

  LLVMValueRef size_loc = field_loc(c, offset_addr, t->structure,
    c->intptr, 1);
  LLVMBuildStore(c->builder, size, size_loc);

  LLVMValueRef alloc_loc = field_loc(c, offset_addr, t->structure,
    c->intptr, 2);
  LLVMBuildStore(c->builder, size, alloc_loc);

  // Write the pointer.
  LLVMValueRef ptr = field_value(c, object, 3);

  // The resulting offset will only be invalid (i.e. have the high bit set) if
  // the size is zero. For an opaque array, we don't serialise the contents,
  // so we don't get here, so we don't end up with an invalid offset.
  LLVMValueRef args[5];
  args[0] = ctx;
  args[1] = ptr;
  LLVMValueRef ptr_offset = gencall_runtime(c, "pony_serialise_offset",
    args, 2, "");

  LLVMValueRef ptr_loc = field_loc(c, offset_addr, t->structure, c->intptr, 3);
  LLVMBuildStore(c->builder, ptr_offset, ptr_loc);

  LLVMValueRef ptr_offset_addr = LLVMBuildAdd(c->builder, ptr_offset,
    LLVMBuildPtrToInt(c->builder, addr, c->intptr, ""), "");

  // Serialise elements.
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);

  size_t abisize = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, abisize, false);

  if((t_elem->underlying == TK_PRIMITIVE) && (t_elem->primitive != NULL))
  {
    // memcpy machine words
    args[0] = LLVMBuildIntToPtr(c->builder, ptr_offset_addr, c->void_ptr, "");
    args[1] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");
    args[2] = LLVMBuildMul(c->builder, size, l_size, "");
    args[3] = LLVMConstInt(c->i32, 1, false);
    args[4] = LLVMConstInt(c->i1, 0, false);
    if(target_is_ilp32(c->opt->triple))
    {
      gencall_runtime(c, "llvm.memcpy.p0i8.p0i8.i32", args, 5, "");
    } else {
      gencall_runtime(c, "llvm.memcpy.p0i8.p0i8.i64", args, 5, "");
    }
  } else {
    ptr = LLVMBuildBitCast(c->builder, ptr,
      LLVMPointerType(t_elem->use_type, 0), "");

    LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
    LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
    LLVMBasicBlockRef body_block = codegen_block(c, "body");
    LLVMBasicBlockRef post_block = codegen_block(c, "post");

    LLVMValueRef offset_var = LLVMBuildAlloca(c->builder, c->intptr, "");
    LLVMBuildStore(c->builder, ptr_offset_addr, offset_var);

    LLVMBuildBr(c->builder, cond_block);

    // While the index is less than the size, serialise an element. The
    // initial index when coming from the entry block is zero.
    LLVMPositionBuilderAtEnd(c->builder, cond_block);
    LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");
    LLVMValueRef zero = LLVMConstInt(c->intptr, 0, false);
    LLVMAddIncoming(phi, &zero, &entry_block, 1);
    LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, size, "");
    LLVMBuildCondBr(c->builder, test, body_block, post_block);

    // The phi node is the index. Get the element and serialise it.
    LLVMPositionBuilderAtEnd(c->builder, body_block);
    LLVMValueRef elem_ptr = LLVMBuildGEP(c->builder, ptr, &phi, 1, "");

    ptr_offset_addr = LLVMBuildLoad(c->builder, offset_var, "");
    genserialise_element(c, t_elem, false, ctx, elem_ptr, ptr_offset_addr);
    ptr_offset_addr = LLVMBuildAdd(c->builder, ptr_offset_addr, l_size, "");
    LLVMBuildStore(c->builder, ptr_offset_addr, offset_var);

    // Add one to the phi node and branch back to the cond block.
    LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
    LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
    body_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &inc, &body_block, 1);
    LLVMBuildBr(c->builder, cond_block);

    LLVMPositionBuilderAtEnd(c->builder, post_block);
  }

  LLVMBuildBr(c->builder, post_block);
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

void genprim_array_deserialise(compile_t* c, reach_type_t* t)
{
  // Generate the deserisalise function.
  t->deserialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->trace_type);

  codegen_startfun(c, t->deserialise_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->deserialise_fn, LLVMCCallConv);
  LLVMSetLinkage(t->deserialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->deserialise_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->deserialise_fn, 1);

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->structure_ptr,
    "");
  gendeserialise_typeid(c, t, object);

  // Deserialise the array contents.
  LLVMValueRef alloc = field_value(c, object, 2);
  LLVMValueRef ptr_offset = field_value(c, object, 3);
  ptr_offset = LLVMBuildPtrToInt(c->builder, ptr_offset, c->intptr, "");

  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  reach_type_t* t_elem = reach_type(c->reach, typearg);
  size_t abisize = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, abisize, false);

  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = ptr_offset;
  args[2] = LLVMBuildMul(c->builder, alloc, l_size, "");
  LLVMValueRef ptr = gencall_runtime(c, "pony_deserialise_block", args, 3, "");

  LLVMValueRef ptr_loc = LLVMBuildStructGEP(c->builder, object, 3, "");
  LLVMBuildStore(c->builder, ptr, ptr_loc);

  if((t_elem->underlying == TK_PRIMITIVE) && (t_elem->primitive != NULL))
  {
    // Do nothing. A memcpy is sufficient.
  } else {
    LLVMValueRef size = field_value(c, object, 1);
    ptr = LLVMBuildBitCast(c->builder, ptr,
      LLVMPointerType(t_elem->use_type, 0), "");

    LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
    LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
    LLVMBasicBlockRef body_block = codegen_block(c, "body");
    LLVMBasicBlockRef post_block = codegen_block(c, "post");

    LLVMBuildBr(c->builder, cond_block);

    // While the index is less than the size, deserialise an element. The
    // initial index when coming from the entry block is zero.
    LLVMPositionBuilderAtEnd(c->builder, cond_block);
    LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");
    LLVMValueRef zero = LLVMConstInt(c->intptr, 0, false);
    LLVMAddIncoming(phi, &zero, &entry_block, 1);
    LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, size, "");
    LLVMBuildCondBr(c->builder, test, body_block, post_block);

    // The phi node is the index. Get the element and deserialise it.
    LLVMPositionBuilderAtEnd(c->builder, body_block);
    LLVMValueRef elem_ptr = LLVMBuildGEP(c->builder, ptr, &phi, 1, "");
    gendeserialise_element(c, t_elem, false, ctx, elem_ptr);

    // Add one to the phi node and branch back to the cond block.
    LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
    LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
    body_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &inc, &body_block, 1);
    LLVMBuildBr(c->builder, cond_block);

    LLVMPositionBuilderAtEnd(c->builder, post_block);
  }

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

void genprim_string_serialise_trace(compile_t* c, reach_type_t* t)
{
  // Generate the serialise_trace function.
  t->serialise_trace_fn = codegen_addfun(c, genname_serialise_trace(t->name),
    c->serialise_type);

  codegen_startfun(c, t->serialise_trace_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->serialise_trace_fn, LLVMCCallConv);
  LLVMSetLinkage(t->serialise_trace_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->serialise_trace_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->serialise_trace_fn, 1);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->use_type, "");

  // Read the size.
  LLVMValueRef size = field_value(c, object, 1);
  LLVMValueRef alloc = LLVMBuildAdd(c->builder, size,
    LLVMConstInt(c->intptr, 1, false), "");

  // Reserve space for the contents.
  LLVMValueRef ptr = field_value(c, object, 3);

  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = ptr;
  args[2] = alloc;
  gencall_runtime(c, "pony_serialise_reserve", args, 3, "");

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

void genprim_string_serialise(compile_t* c, reach_type_t* t)
{
  // Generate the serialise function.
  t->serialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->serialise_type);

  codegen_startfun(c, t->serialise_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->serialise_fn, LLVMCCallConv);
  LLVMSetLinkage(t->serialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->serialise_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->serialise_fn, 1);
  LLVMValueRef addr = LLVMGetParam(t->serialise_fn, 2);
  LLVMValueRef offset = LLVMGetParam(t->serialise_fn, 3);
  LLVMValueRef mut = LLVMGetParam(t->serialise_fn, 4);

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->structure_ptr,
    "");
  LLVMValueRef offset_addr = LLVMBuildAdd(c->builder,
    LLVMBuildPtrToInt(c->builder, addr, c->intptr, ""), offset, "");

  genserialise_typeid(c, t, offset_addr);

  // Don't serialise our contents if we are opaque.
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntNE, mut,
    LLVMConstInt(c->i32, PONY_TRACE_OPAQUE, false), "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);
  LLVMPositionBuilderAtEnd(c->builder, body_block);

  // Write the size, and rewrite alloc to be size + 1.
  LLVMValueRef size = field_value(c, object, 1);
  LLVMValueRef size_loc = field_loc(c, offset_addr, t->structure,
    c->intptr, 1);
  LLVMBuildStore(c->builder, size, size_loc);

  LLVMValueRef alloc = LLVMBuildAdd(c->builder, size,
    LLVMConstInt(c->intptr, 1, false), "");
  LLVMValueRef alloc_loc = field_loc(c, offset_addr, t->structure,
    c->intptr, 2);
  LLVMBuildStore(c->builder, alloc, alloc_loc);

  // Write the pointer.
  LLVMValueRef ptr = field_value(c, object, 3);

  LLVMValueRef args[5];
  args[0] = ctx;
  args[1] = ptr;
  LLVMValueRef ptr_offset = gencall_runtime(c, "pony_serialise_offset",
    args, 2, "");

  LLVMValueRef ptr_loc = field_loc(c, offset_addr, t->structure, c->intptr, 3);
  LLVMBuildStore(c->builder, ptr_offset, ptr_loc);

  // Serialise the string contents.
  LLVMValueRef ptr_offset_addr = LLVMBuildAdd(c->builder,
    LLVMBuildPtrToInt(c->builder, addr, c->intptr, ""), ptr_offset, "");

  args[0] = LLVMBuildIntToPtr(c->builder, ptr_offset_addr, c->void_ptr, "");
  args[1] = LLVMBuildBitCast(c->builder, field_value(c, object, 3),
    c->void_ptr, "");
  args[2] = alloc;
  args[3] = LLVMConstInt(c->i32, 1, false);
  args[4] = LLVMConstInt(c->i1, 0, false);

  if(target_is_ilp32(c->opt->triple))
  {
    gencall_runtime(c, "llvm.memcpy.p0i8.p0i8.i32", args, 5, "");
  } else {
    gencall_runtime(c, "llvm.memcpy.p0i8.p0i8.i64", args, 5, "");
  }

  LLVMBuildBr(c->builder, post_block);
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

void genprim_string_deserialise(compile_t* c, reach_type_t* t)
{
  // Generate the deserisalise function.
  t->deserialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->trace_type);

  codegen_startfun(c, t->deserialise_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->deserialise_fn, LLVMCCallConv);
  LLVMSetLinkage(t->deserialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->deserialise_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->deserialise_fn, 1);

  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->structure_ptr,
    "");
  gendeserialise_typeid(c, t, object);

  // Deserialise the string contents.
  LLVMValueRef alloc = field_value(c, object, 2);
  LLVMValueRef ptr_offset = field_value(c, object, 3);
  ptr_offset = LLVMBuildPtrToInt(c->builder, ptr_offset, c->intptr, "");

  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = ptr_offset;
  args[2] = alloc;
  LLVMValueRef ptr_addr = gencall_runtime(c, "pony_deserialise_block", args, 3,
    "");

  LLVMValueRef ptr = LLVMBuildStructGEP(c->builder, object, 3, "");
  LLVMBuildStore(c->builder, ptr_addr, ptr);

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

static void platform_freebsd(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("freebsd");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_freebsd(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_linux(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("linux");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_linux(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_osx(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("osx");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_macosx(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_windows(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("windows");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_windows(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_x86(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("x86");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_x86(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_arm(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("arm");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_arm(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_lp64(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("lp64");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_lp64(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_llp64(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("llp64");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_llp64(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_ilp32(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("ilp32");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_ilp32(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_native128(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("native128");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_native128(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void platform_debug(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("debug");
  start_function(c, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result = LLVMConstInt(c->ibool, !c->opt->release, false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

void genprim_platform_methods(compile_t* c, reach_type_t* t)
{
  platform_freebsd(c, t);
  platform_linux(c, t);
  platform_osx(c, t);
  platform_windows(c, t);
  platform_x86(c, t);
  platform_arm(c, t);
  platform_lp64(c, t);
  platform_llp64(c, t);
  platform_ilp32(c, t);
  platform_native128(c, t);
  platform_debug(c, t);
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

static void number_conversion(compile_t* c, num_conv_t* from, num_conv_t* to,
  bool native128)
{
  if(!native128 &&
    ((from->is_float && (to->size > 64)) ||
    (to->is_float && (from->size > 64)))
    )
  {
    return;
  }

  reach_type_t* t = reach_type_name(c->reach, from->type_name);

  if(t == NULL)
    return;

  FIND_METHOD(to->fun_name);
  start_function(c, m, to->type, &from->type, 1);

  LLVMValueRef arg = LLVMGetParam(m->func, 0);
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

  BOX_FUNCTION();
}

static void number_conversions(compile_t* c)
{
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

  num_conv_t* conv = NULL;

  if(target_is_ilp32(c->opt->triple))
    conv = ilp32_conv;
  else if(target_is_lp64(c->opt->triple))
    conv = lp64_conv;
  else if(target_is_llp64(c->opt->triple))
    conv = llp64_conv;

  assert(conv != NULL);
  bool native128 = target_is_native128(c->opt->triple);

  for(num_conv_t* from = conv; from->type_name != NULL; from++)
  {
    for(num_conv_t* to = conv; to->type_name != NULL; to++)
      number_conversion(c, from, to, native128);
  }
}

static void f32__nan(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_nan");
  start_function(c, m, c->f32, &c->f32, 1);

  LLVMValueRef result = LLVMConstNaN(c->f32);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f32_from_bits(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("from_bits");

  LLVMTypeRef params[2];
  params[0] = c->f32;
  params[1] = c->i32;
  start_function(c, m, c->f32, params, 2);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 1),
    c->f32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f32_bits(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("bits");
  start_function(c, m, c->i32, &c->f32, 1);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 0),
    c->i32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void f64__nan(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_nan");
  start_function(c, m, c->f64, &c->f64, 1);

  LLVMValueRef result = LLVMConstNaN(c->f64);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f64_from_bits(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("from_bits");

  LLVMTypeRef params[2];
  params[0] = c->f64;
  params[1] = c->i64;
  start_function(c, m, c->f64, params, 2);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 1),
    c->f64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f64_bits(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("bits");
  start_function(c, m, c->i64, &c->f64, 1);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 0),
    c->i64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);

  BOX_FUNCTION();
}

static void fp_intrinsics(compile_t* c)
{
  reach_type_t* t;

  if((t = reach_type_name(c->reach, "F32")) != NULL)
  {
    f32__nan(c, t);
    f32_from_bits(c, t);
    f32_bits(c, t);
  }

  if((t = reach_type_name(c->reach, "F64")) != NULL)
  {
    f64__nan(c, t);
    f64_from_bits(c, t);
    f64_bits(c, t);
  }
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
    codegen_startfun(c, fun, NULL, NULL);

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
    LLVMValueRef rdtscp = LLVMAddFunction(c->module, "llvm.x86.rdtscp",
      f_type);

    // i64 @internal.x86.rdtscp(i32*)
    LLVMTypeRef i32_ptr = LLVMPointerType(c->i32, 0);
    f_type = LLVMFunctionType(c->i64, &i32_ptr, 1, false);
    LLVMValueRef fun = codegen_addfun(c, "internal.x86.rdtscp", f_type);
    LLVMSetFunctionCallConv(fun, LLVMCCallConv);
    codegen_startfun(c, fun, NULL, NULL);

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
  fp_intrinsics(c);
  make_cpuid(c);
  make_rdtscp(c);
}

void genprim_reachable_init(compile_t* c, ast_t* program)
{
  // Look for primitives in all packages that have _init or _final methods.
  // Mark them as reachable.
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
            ast_t* finit = ast_get(entity, c->str__init, NULL);
            ast_t* ffinal = ast_get(entity, c->str__final, NULL);

            if(finit != NULL)
            {
              reach(c->reach, type, c->str__init, NULL, c->opt);
              ast_free_unattached(finit);
            }

            if(ffinal != NULL)
            {
              reach(c->reach, type, c->str__final, NULL, c->opt);
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

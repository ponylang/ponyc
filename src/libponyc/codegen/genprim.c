#include "genprim.h"
#include "gencall.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include "genopt.h"
#include "genserialise.h"
#include "gentrace.h"
#include "../pkg/package.h"
#include "../pkg/platformfuns.h"
#include "../pkg/program.h"
#include "../pass/names.h"
#include "../type/assemble.h"
#include "../type/cap.h"
#include "../../libponyrt/mem/heap.h"
#include "ponyassert.h"

#include <string.h>

#define FIND_METHOD(name, cap) \
  const char* strtab_name = stringtab(name); \
  reach_method_t* m = reach_method(t, cap, strtab_name, NULL); \
  if(m == NULL) return; \
  m->intrinsic = true; \
  compile_type_t* c_t = (compile_type_t*)t->c_type; \
  (void)c_t; \
  compile_method_t* c_m = (compile_method_t*)m->c_method; \
  (void)c_m;

#define BOX_FUNCTION(gen, gen_data) \
  gen(c, gen_data, TK_BOX); \
  gen(c, gen_data, TK_REF); \
  gen(c, gen_data, TK_VAL);

#define GENERIC_FUNCTION(name, gen) \
  generic_function(c, t, stringtab(name), gen);

typedef void (*generate_gen_fn)(compile_t*, reach_type_t*, reach_method_t*);

static void start_function(compile_t* c, reach_type_t* t, reach_method_t* m,
  LLVMTypeRef result, LLVMTypeRef* params, unsigned count)
{
  compile_method_t* c_m = (compile_method_t*)m->c_method;
  c_m->func_type = LLVMFunctionType(result, params, count, false);
  c_m->func = codegen_addfun(c, m->full_name, c_m->func_type, true);
  genfun_param_attrs(c, t, m, c_m->func);
  codegen_startfun(c, c_m->func, NULL, NULL, NULL, false);
}

static void generic_function(compile_t* c, reach_type_t* t, const char* name,
  generate_gen_fn gen)
{
  reach_method_name_t* n = reach_method_name(t, name);

  if(n == NULL)
    return;

  size_t i = HASHMAP_BEGIN;
  reach_method_t* m;
  while((m = reach_methods_next(&n->r_methods, &i)) != NULL)
  {
    m->intrinsic = true;
    gen(c, t, m);
  }
}

static LLVMValueRef field_loc(compile_t* c, LLVMValueRef offset,
  LLVMTypeRef structure, int index)
{
  LLVMValueRef size = LLVMConstInt(c->intptr,
    LLVMOffsetOfElement(c->target_data, structure, index), false);
  return LLVMBuildInBoundsGEP2(c->builder, c->i8, offset, &size, 1, "");
}

static LLVMValueRef field_value(compile_t* c, LLVMTypeRef struct_type,
  LLVMValueRef object, int index)
{
  LLVMTypeRef field_type = LLVMStructGetTypeAtIndex(struct_type, index);
  LLVMValueRef field = LLVMBuildStructGEP2(c->builder, struct_type, object,
    index, "");
  return LLVMBuildLoad2(c->builder, field_type, field, "");
}

static void pointer_create(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("create", TK_NONE);
  start_function(c, t, m, c_t->use_type, &c_t->use_type, 1);

  LLVMValueRef result = LLVMConstNull(c_t->use_type);

  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void pointer_alloc(compile_t* c, reach_type_t* t,
  compile_type_t* t_elem)
{
  FIND_METHOD("_alloc", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = c_t->use_type;
  params[1] = c->intptr;
  start_function(c, t, m, c_t->use_type, params, 2);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->mem_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVM_DECLARE_ATTRIBUTEREF(noalias_attr, noalias, 0);
  LLVM_DECLARE_ATTRIBUTEREF(deref_attr, dereferenceable_or_null, size);
  LLVM_DECLARE_ATTRIBUTEREF(align_attr, align, HEAP_MIN);

  LLVMAddAttributeAtIndex(c_m->func, LLVMAttributeReturnIndex, noalias_attr);
  LLVMAddAttributeAtIndex(c_m->func, LLVMAttributeReturnIndex, deref_attr);
  LLVMAddAttributeAtIndex(c_m->func, LLVMAttributeReturnIndex, align_attr);

  LLVMValueRef len = LLVMGetParam(c_m->func, 1);
  LLVMValueRef args[2];
  args[0] = codegen_ctx(c);
  args[1] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 2, "");
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void pointer_realloc(compile_t* c, reach_type_t* t,
  compile_type_t* t_elem)
{
  FIND_METHOD("_realloc", TK_NONE);

  LLVMTypeRef params[3];
  params[0] = c_t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, t, m, c_t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->mem_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVM_DECLARE_ATTRIBUTEREF(noalias_attr, noalias, 0);
  LLVM_DECLARE_ATTRIBUTEREF(deref_attr, dereferenceable_or_null, size);
  LLVM_DECLARE_ATTRIBUTEREF(align_attr, align, HEAP_MIN);

  LLVMAddAttributeAtIndex(c_m->func, LLVMAttributeReturnIndex, noalias_attr);
  LLVMAddAttributeAtIndex(c_m->func, LLVMAttributeReturnIndex, deref_attr);
  LLVMAddAttributeAtIndex(c_m->func, LLVMAttributeReturnIndex, align_attr);

  LLVMValueRef args[4];
  args[0] = codegen_ctx(c);
  args[1] = LLVMGetParam(c_m->func, 0);

  LLVMValueRef len = LLVMGetParam(c_m->func, 1);
  args[2] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef copy = LLVMGetParam(c_m->func, 2);
  args[3] = LLVMBuildMul(c->builder, copy, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_realloc", args, 4, "");
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void pointer_unsafe(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_unsafe", TK_NONE);
  start_function(c, t, m, c_t->use_type, &c_t->use_type, 1);

  genfun_build_ret(c, LLVMGetParam(c_m->func, 0));
  codegen_finishfun(c);
}

static void pointer_convert(compile_t* c, reach_type_t* t, reach_method_t* m)
{
  m->intrinsic = true;
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;

  compile_type_t* t_result = (compile_type_t*)m->result->c_type;

  start_function(c, t, m, t_result->use_type, &c_t->use_type, 1);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  genfun_build_ret(c, ptr);
  codegen_finishfun(c);
}

static void pointer_apply(compile_t* c, void* data, token_id cap)
{
  reach_type_t* t = ((reach_type_t**)data)[0];
  reach_type_t* t_elem = ((reach_type_t**)data)[1];
  compile_type_t* c_t_elem = (compile_type_t*)t_elem->c_type;

  FIND_METHOD("_apply", cap);

  LLVMTypeRef params[2];
  params[0] = c_t->use_type;
  params[1] = c->intptr;
  start_function(c, t, m, c_t_elem->use_type, params, 2);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef index = LLVMGetParam(c_m->func, 1);

  LLVMValueRef loc = LLVMBuildInBoundsGEP2(c->builder, c_t_elem->mem_type, ptr,
    &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad2(c->builder, c_t_elem->mem_type, loc, "");

  ast_t* tcap = ast_childidx(t->ast, 3);
  token_id tmp_cap = ast_id(tcap);
  ast_setid(tcap, cap);

  ast_setid(tcap, tmp_cap);

  result = gen_assign_cast(c, c_t_elem->use_type, result, t_elem->ast_cap);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void pointer_update(compile_t* c, reach_type_t* t,
  reach_type_t* t_elem)
{
  compile_type_t* c_t_elem = (compile_type_t*)t_elem->c_type;

  FIND_METHOD("_update", TK_NONE);

  LLVMTypeRef params[3];
  params[0] = c_t->use_type;
  params[1] = c->intptr;
  params[2] = c_t_elem->use_type;
  start_function(c, t, m, c_t_elem->use_type, params, 3);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef index = LLVMGetParam(c_m->func, 1);

  LLVMValueRef loc = LLVMBuildInBoundsGEP2(c->builder, c_t_elem->mem_type, ptr,
    &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad2(c->builder, c_t_elem->mem_type, loc, "");

  LLVMValueRef value = LLVMGetParam(c_m->func, 2);
  value = gen_assign_cast(c, c_t_elem->mem_type, value, t_elem->ast_cap);
  LLVMBuildStore(c->builder, value, loc);

  result = gen_assign_cast(c, c_t_elem->use_type, result, t_elem->ast_cap);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void pointer_offset(compile_t* c, void* data, token_id cap)
{
  reach_type_t* t = ((reach_type_t**)data)[0];
  compile_type_t* t_elem = ((compile_type_t**)data)[1];

  FIND_METHOD("_offset", cap);

  LLVMTypeRef params[3];
  params[0] = c_t->use_type;
  params[1] = c->intptr;
  start_function(c, t, m, c_t->use_type, params, 2);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef n = LLVMGetParam(c_m->func, 1);

  // Return ptr + (n * sizeof(len)).
  LLVMValueRef result = LLVMBuildInBoundsGEP2(c->builder, t_elem->mem_type, ptr,
    &n, 1, "");

  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void pointer_element_size(compile_t* c, reach_type_t* t,
  compile_type_t* t_elem)
{
  FIND_METHOD("_element_size", TK_NONE);
  start_function(c, t, m, c->intptr, &c_t->use_type, 1);

  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->mem_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  genfun_build_ret(c, l_size);
  codegen_finishfun(c);
}

static void pointer_insert(compile_t* c, reach_type_t* t,
  compile_type_t* t_elem)
{
  FIND_METHOD("_insert", TK_NONE);

  LLVMTypeRef params[3];
  params[0] = c_t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, t, m, c_t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->mem_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef n = LLVMGetParam(c_m->func, 1);
  LLVMValueRef len = LLVMGetParam(c_m->func, 2);

  LLVMValueRef dst = LLVMBuildInBoundsGEP2(c->builder, t_elem->mem_type, ptr,
    &n, 1, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  // llvm.memmove.*(ptr + (n * sizeof(elem)), ptr, len * sizeof(elem))
  gencall_memmove(c, dst, ptr, elen);

  // Return ptr.
  genfun_build_ret(c, ptr);
  codegen_finishfun(c);
}

static void pointer_delete(compile_t* c, reach_type_t* t,
  reach_type_t* t_elem)
{
  compile_type_t* c_t_elem = (compile_type_t*)t_elem->c_type;

  FIND_METHOD("_delete", TK_NONE);

  LLVMTypeRef params[3];
  params[0] = c_t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, t, m, c_t_elem->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, c_t_elem->mem_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef n = LLVMGetParam(c_m->func, 1);
  LLVMValueRef len = LLVMGetParam(c_m->func, 2);

  LLVMValueRef result = LLVMBuildLoad2(c->builder, c_t_elem->mem_type, ptr, "");

  LLVMValueRef src = LLVMBuildInBoundsGEP2(c->builder, c_t_elem->mem_type, ptr,
    &n, 1, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  // llvm.memmove.*(ptr, ptr + (n * sizeof(elem)), len * sizeof(elem))
  gencall_memmove(c, ptr, src, elen);

  // Return ptr[0].
  result = gen_assign_cast(c, c_t_elem->use_type, result, t_elem->ast_cap);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void pointer_copy_to(compile_t* c, void* data, token_id cap)
{
  reach_type_t* t = ((reach_type_t**)data)[0];
  compile_type_t* t_elem = ((compile_type_t**)data)[1];

  FIND_METHOD("_copy_to", cap);

  LLVMTypeRef params[3];
  params[0] = c_t->use_type;
  params[1] = c_t->use_type;
  params[2] = c->intptr;
  start_function(c, t, m, c_t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->mem_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(c_m->func, 1);
  LLVMValueRef n = LLVMGetParam(c_m->func, 2);
  LLVMValueRef elen = LLVMBuildMul(c->builder, n, l_size, "");

  // llvm.memcpy.*(ptr2, ptr, n * sizeof(elem), 1, 0)
  gencall_memcpy(c, ptr2, ptr, elen);

  genfun_build_ret(c, ptr);
  codegen_finishfun(c);
}

static void pointer_usize(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("usize", TK_NONE);
  start_function(c, t, m, c->intptr, &c_t->use_type, 1);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef result = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");

  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void pointer_is_null(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("is_null", TK_NONE);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, ptr, "");

  genfun_build_ret(c, test);
  codegen_finishfun(c);
}

static void pointer_eq(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("eq", TK_NONE);
  LLVMTypeRef params[2];
  params[0] = c_t->use_type;
  params[1] = c_t->use_type;
  start_function(c, t, m, c->i1, params, 2);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(c_m->func, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, ptr, ptr2, "");

  genfun_build_ret(c, test);
  codegen_finishfun(c);
}

static void pointer_lt(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("lt", TK_NONE);
  LLVMTypeRef params[2];
  params[0] = c_t->use_type;
  params[1] = c_t->use_type;
  start_function(c, t, m, c->i1, params, 2);

  LLVMValueRef ptr = LLVMGetParam(c_m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(c_m->func, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, ptr, ptr2, "");

  genfun_build_ret(c, test);
  codegen_finishfun(c);
}

void genprim_pointer_methods(compile_t* c, reach_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);
  compile_type_t* c_t_elem = (compile_type_t*)t_elem->c_type;

  void* box_args[2];
  box_args[0] = t;
  box_args[1] = t_elem;

  void* c_box_args[2];
  c_box_args[0] = t;
  c_box_args[1] = c_t_elem;

  pointer_create(c, t);
  pointer_alloc(c, t, c_t_elem);

  pointer_realloc(c, t, c_t_elem);
  pointer_unsafe(c, t);
  GENERIC_FUNCTION("_convert", pointer_convert);
  BOX_FUNCTION(pointer_apply, box_args);
  pointer_update(c, t, t_elem);
  BOX_FUNCTION(pointer_offset, c_box_args);
  pointer_element_size(c, t, c_t_elem);
  pointer_insert(c, t, c_t_elem);
  pointer_delete(c, t, t_elem);
  BOX_FUNCTION(pointer_copy_to, c_box_args);
  pointer_usize(c, t);
  pointer_is_null(c, t);
  pointer_eq(c, t);
  pointer_lt(c, t);
}

static void nullable_pointer_create(compile_t* c, reach_type_t* t, compile_type_t* t_elem)
{
  FIND_METHOD("create", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = c_t->use_type;
  params[1] = t_elem->use_type;
  start_function(c, t, m, c_t->use_type, params, 2);

  genfun_build_ret(c, LLVMGetParam(c_m->func, 1));
  codegen_finishfun(c);
}

static void nullable_pointer_none(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("none", TK_NONE);
  start_function(c, t, m, c_t->use_type, &c_t->use_type, 1);

  genfun_build_ret(c, LLVMConstNull(c_t->use_type));
  codegen_finishfun(c);
}

static void nullable_pointer_apply(compile_t* c, void* data, token_id cap)
{
  // Returns the receiver if it isn't null.
  reach_type_t* t = ((reach_type_t**)data)[0];
  compile_type_t* t_elem = ((compile_type_t**)data)[1];

  FIND_METHOD("apply", cap);
  start_function(c, t, m, t_elem->use_type, &c_t->use_type, 1);

  LLVMValueRef result = LLVMGetParam(c_m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, result, "");

  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  LLVMPositionBuilderAtEnd(c->builder, is_false);
  genfun_build_ret(c, result);

  LLVMPositionBuilderAtEnd(c->builder, is_true);
  gencall_error(c);

  codegen_finishfun(c);
}

static void nullable_pointer_is_none(compile_t* c, reach_type_t* t, token_id cap)
{
  // Returns true if the receiver is null.
  FIND_METHOD("is_none", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef receiver = LLVMGetParam(c_m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, receiver, "");

  genfun_build_ret(c, test);
  codegen_finishfun(c);
}

void genprim_nullable_pointer_methods(compile_t* c, reach_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  compile_type_t* t_elem =
    (compile_type_t*)reach_type(c->reach, typearg)->c_type;

  void* box_args[2];
  box_args[0] = t;
  box_args[1] = t_elem;

  nullable_pointer_create(c, t, t_elem);
  nullable_pointer_none(c, t);
  BOX_FUNCTION(nullable_pointer_apply, box_args);
  BOX_FUNCTION(nullable_pointer_is_none, t);
}

static void donotoptimise_apply(compile_t* c, reach_type_t* t,
  reach_method_t* m)
{
  m->intrinsic = true;
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  compile_method_t* c_m = (compile_method_t*)m->c_method;

  ast_t* typearg = ast_child(m->typeargs);
  compile_type_t* t_elem =
    (compile_type_t*)reach_type(c->reach, typearg)->c_type;
  compile_type_t* t_result = (compile_type_t*)m->result->c_type;

  LLVMTypeRef params[2];
  params[0] = c_t->use_type;
  params[1] = t_elem->use_type;

  start_function(c, t, m, t_result->use_type, params, 2);

  LLVMValueRef obj = LLVMGetParam(c_m->func, 1);
  LLVMTypeRef void_fn = LLVMFunctionType(c->void_type, &t_elem->use_type, 1,
    false);
  LLVMValueRef asmstr = LLVMConstInlineAsm(void_fn, "", "imr,~{memory}", true,
    false);
  LLVMValueRef call = LLVMBuildCall2(c->builder, void_fn, asmstr, &obj, 1, "");

  bool is_ptr = LLVMGetTypeKind(t_elem->use_type) == LLVMPointerTypeKind;

  LLVM_DECLARE_ATTRIBUTEREF(nounwind_attr, nounwind, 0);
  LLVM_DECLARE_ATTRIBUTEREF(readonly_attr, readonly, 0);
  LLVM_DECLARE_ATTRIBUTEREF(inacc_or_arg_mem_attr, memory,
    LLVM_MEMORYEFFECTS_ARG(LLVM_MEMORYEFFECTS_READWRITE) |
    LLVM_MEMORYEFFECTS_INACCESSIBLEMEM(LLVM_MEMORYEFFECTS_READWRITE));

  LLVMAddCallSiteAttribute(call, LLVMAttributeFunctionIndex, nounwind_attr);

  if(is_ptr)
    LLVMAddCallSiteAttribute(call, 1, readonly_attr);

  LLVMAddCallSiteAttribute(call, LLVMAttributeFunctionIndex,
    inacc_or_arg_mem_attr);

  genfun_build_ret(c, t_result->instance);
  codegen_finishfun(c);
}

static void donotoptimise_observe(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("observe", cap);
  compile_type_t* t_result = (compile_type_t*)m->result->c_type;

  start_function(c, t, m, t_result->use_type, &c_t->use_type, 1);

  LLVMTypeRef void_fn = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMValueRef asmstr = LLVMConstInlineAsm(void_fn, "", "~{memory}", true,
    false);
  LLVMValueRef call = LLVMBuildCall2(c->builder, void_fn, asmstr, NULL, 0, "");

  LLVM_DECLARE_ATTRIBUTEREF(nounwind_attr, nounwind, 0);
  LLVM_DECLARE_ATTRIBUTEREF(inacc_or_arg_mem_attr, memory,
    LLVM_MEMORYEFFECTS_ARG(LLVM_MEMORYEFFECTS_READWRITE) |
    LLVM_MEMORYEFFECTS_INACCESSIBLEMEM(LLVM_MEMORYEFFECTS_READWRITE));

  LLVMAddCallSiteAttribute(call, LLVMAttributeFunctionIndex, nounwind_attr);
  LLVMAddCallSiteAttribute(call, LLVMAttributeFunctionIndex,
    inacc_or_arg_mem_attr);

  genfun_build_ret(c, t_result->instance);
  codegen_finishfun(c);
}

void genprim_donotoptimise_methods(compile_t* c, reach_type_t* t)
{
  GENERIC_FUNCTION("apply", donotoptimise_apply);
  BOX_FUNCTION(donotoptimise_observe, t);
}

static void trace_array_elements(compile_t* c, reach_type_t* t,
  LLVMValueRef ctx, LLVMValueRef object, LLVMValueRef pointer)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // Get the type argument for the array. This will be used to generate the
  // per-element trace call.
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  if(!gentrace_needed(c, typearg, typearg))
    return;

  reach_type_t* t_elem = reach_type(c->reach, typearg);
  compile_type_t* c_t_elem = (compile_type_t*)t_elem->c_type;

  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  // Read the size.
  LLVMValueRef size = field_value(c, c_t->structure, object, 1);
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
  LLVMValueRef elem_ptr = LLVMBuildInBoundsGEP2(c->builder, c_t_elem->mem_type,
    pointer, &phi, 1, "");
  LLVMValueRef elem = LLVMBuildLoad2(c->builder, c_t_elem->mem_type, elem_ptr,
    "");
  elem = gen_assign_cast(c, c_t_elem->use_type, elem, typearg);
  gentrace(c, ctx, elem, elem, typearg, typearg);

  // Add one to the phi node and branch back to the cond block.
  LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  body_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, post_block);
}

void genprim_array_trace(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  codegen_startfun(c, c_t->trace_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->trace_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->trace_fn, LLVMExternalLinkage);
  LLVMValueRef ctx = LLVMGetParam(c_t->trace_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->trace_fn, 1);

  // Read the base pointer.
  LLVMValueRef pointer = field_value(c, c_t->structure, object, 3);

  // Trace the base pointer.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = pointer;
  gencall_runtime(c, "pony_trace", args, 2, "");

  trace_array_elements(c, t, ctx, object, pointer);
  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

void genprim_array_serialise_trace(compile_t* c, reach_type_t* t)
{
  // Generate the serialise_trace function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->serialise_trace_fn = codegen_addfun(c, genname_serialise_trace(t->name),
    c->trace_fn, true);

  codegen_startfun(c, c_t->serialise_trace_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->serialise_trace_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->serialise_trace_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->serialise_trace_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->serialise_trace_fn, 1);

  LLVMValueRef size = field_value(c, c_t->structure, object, 1);

  LLVMBasicBlockRef trace_block = codegen_block(c, "trace");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  LLVMValueRef cond = LLVMBuildICmp(c->builder, LLVMIntNE, size,
    LLVMConstInt(c->intptr, 0, false), "");
  LLVMBuildCondBr(c->builder, cond, trace_block, post_block);

  LLVMPositionBuilderAtEnd(c->builder, trace_block);

  // Calculate the size of the element type.
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  compile_type_t* t_elem =
    (compile_type_t*)reach_type(c->reach, typearg)->c_type;

  size_t abisize = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, abisize, false);

  // Reserve space for the array elements.
  LLVMValueRef pointer = field_value(c, c_t->structure, object, 3);

  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = pointer;
  args[2] = LLVMBuildMul(c->builder, size, l_size, "");
  gencall_runtime(c, "pony_serialise_reserve", args, 3, "");

  // Trace the array elements.
  trace_array_elements(c, t, ctx, object, pointer);

  LLVMBuildBr(c->builder, post_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);

  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

void genprim_array_serialise(compile_t* c, reach_type_t* t)
{
  // Generate the serialise function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->serialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->serialise_fn, true);

  codegen_startfun(c, c_t->serialise_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->serialise_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->serialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->serialise_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->serialise_fn, 1);
  LLVMValueRef addr = LLVMGetParam(c_t->serialise_fn, 2);
  LLVMValueRef offset = LLVMGetParam(c_t->serialise_fn, 3);
  LLVMValueRef mut = LLVMGetParam(c_t->serialise_fn, 4);

  LLVMValueRef offset_addr = LLVMBuildInBoundsGEP2(c->builder, c->i8, addr,
    &offset, 1, "");

  genserialise_serialiseid(c, t, offset_addr);

  // Don't serialise our contents if we are opaque.
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntNE, mut,
    LLVMConstInt(c->i32, PONY_TRACE_OPAQUE, false), "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);
  LLVMPositionBuilderAtEnd(c->builder, body_block);

  // Write the size twice, effectively rewriting alloc to be the same as size.
  LLVMValueRef size = field_value(c, c_t->structure, object, 1);

  LLVMValueRef size_loc = field_loc(c, offset_addr, c_t->structure, 1);
  LLVMBuildStore(c->builder, size, size_loc);

  LLVMValueRef alloc_loc = field_loc(c, offset_addr, c_t->structure, 2);
  LLVMBuildStore(c->builder, size, alloc_loc);

  // Write the pointer.
  LLVMValueRef ptr = field_value(c, c_t->structure, object, 3);

  // The resulting offset will only be invalid (i.e. have the high bit set) if
  // the size is zero. For an opaque array, we don't serialise the contents,
  // so we don't get here, so we don't end up with an invalid offset.
  LLVMValueRef args[5];
  args[0] = ctx;
  args[1] = ptr;
  LLVMValueRef ptr_offset = gencall_runtime(c, "pony_serialise_offset",
    args, 2, "");

  LLVMValueRef ptr_loc = field_loc(c, offset_addr, c_t->structure, 3);
  LLVMBuildStore(c->builder, ptr_offset, ptr_loc);

  LLVMValueRef ptr_offset_addr = LLVMBuildInBoundsGEP2(c->builder, c->i8, addr,
    &ptr_offset, 1, "");

  // Serialise elements.
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);
  compile_type_t* c_t_e = (compile_type_t*)t_elem->c_type;

  size_t abisize = (size_t)LLVMABISizeOfType(c->target_data, c_t_e->mem_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, abisize, false);

  if((t_elem->underlying == TK_PRIMITIVE) && (c_t_e->primitive != NULL))
  {
    // memcpy machine words
    size = LLVMBuildMul(c->builder, size, l_size, "");
    gencall_memcpy(c, ptr_offset_addr, ptr, size);
  } else {
    LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
    LLVMBasicBlockRef cond_elem_block = codegen_block(c, "cond");
    LLVMBasicBlockRef body_elem_block = codegen_block(c, "body");
    LLVMBasicBlockRef post_elem_block = codegen_block(c, "post");

    LLVMValueRef offset_var = LLVMBuildAlloca(c->builder, c->ptr, "");
    LLVMBuildStore(c->builder, ptr_offset_addr, offset_var);

    LLVMBuildBr(c->builder, cond_elem_block);

    // While the index is less than the size, serialise an element. The
    // initial index when coming from the entry block is zero.
    LLVMPositionBuilderAtEnd(c->builder, cond_elem_block);
    LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");
    LLVMValueRef zero = LLVMConstInt(c->intptr, 0, false);
    LLVMAddIncoming(phi, &zero, &entry_block, 1);
    LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, size, "");
    LLVMBuildCondBr(c->builder, test, body_elem_block, post_elem_block);

    // The phi node is the index. Get the element and serialise it.
    LLVMPositionBuilderAtEnd(c->builder, body_elem_block);
    LLVMValueRef elem_ptr = LLVMBuildInBoundsGEP2(c->builder, c_t_e->mem_type,
      ptr, &phi, 1, "");

    ptr_offset_addr = LLVMBuildLoad2(c->builder, c->ptr, offset_var, "");
    genserialise_element(c, t_elem, false, ctx, elem_ptr, ptr_offset_addr);
    ptr_offset_addr = LLVMBuildInBoundsGEP2(c->builder, c->i8, ptr_offset_addr,
      &l_size, 1, "");
    LLVMBuildStore(c->builder, ptr_offset_addr, offset_var);

    // Add one to the phi node and branch back to the cond block.
    LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
    LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
    body_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &inc, &body_block, 1);
    LLVMBuildBr(c->builder, cond_elem_block);

    LLVMMoveBasicBlockAfter(post_elem_block, LLVMGetInsertBlock(c->builder));
    LLVMPositionBuilderAtEnd(c->builder, post_elem_block);
  }

  LLVMBuildBr(c->builder, post_block);
  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

void genprim_array_deserialise(compile_t* c, reach_type_t* t)
{
  // Generate the deserisalise function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->deserialise_fn = codegen_addfun(c, genname_deserialise(t->name),
    c->trace_fn, true);

  codegen_startfun(c, c_t->deserialise_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->deserialise_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->deserialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->deserialise_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->deserialise_fn, 1);
  gendeserialise_serialiseid(c, c_t, object);

  // Deserialise the array contents.
  LLVMValueRef alloc = field_value(c, c_t->structure, object, 2);
  LLVMValueRef ptr_offset = field_value(c, c_t->structure, object, 3);
  ptr_offset = LLVMBuildPtrToInt(c->builder, ptr_offset, c->intptr, "");

  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  reach_type_t* t_elem = reach_type(c->reach, typearg);
  compile_type_t* c_t_e = (compile_type_t*)t_elem->c_type;
  size_t abisize = (size_t)LLVMABISizeOfType(c->target_data, c_t_e->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, abisize, false);

  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = ptr_offset;
  args[2] = LLVMBuildMul(c->builder, alloc, l_size, "");
  LLVMValueRef ptr = gencall_runtime(c, "pony_deserialise_block", args, 3, "");

  LLVMValueRef ptr_loc = LLVMBuildStructGEP2(c->builder, c_t->structure, object,
    3, "");
  LLVMBuildStore(c->builder, ptr, ptr_loc);

  if((t_elem->underlying == TK_PRIMITIVE) && (c_t_e->primitive != NULL))
  {
    // Do nothing. A memcpy is sufficient.
  } else {
    LLVMValueRef size = field_value(c, c_t->structure, object, 1);

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
    LLVMValueRef elem_ptr = LLVMBuildInBoundsGEP2(c->builder, c_t_e->mem_type,
      ptr, &phi, 1, "");
    gendeserialise_element(c, t_elem, false, ctx, elem_ptr);

    // Add one to the phi node and branch back to the cond block.
    LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
    LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
    body_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &inc, &body_block, 1);
    LLVMBuildBr(c->builder, cond_block);

    LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
    LLVMPositionBuilderAtEnd(c->builder, post_block);
  }

  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

void genprim_string_serialise_trace(compile_t* c, reach_type_t* t)
{
  // Generate the serialise_trace function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->serialise_trace_fn = codegen_addfun(c, genname_serialise_trace(t->name),
    c->serialise_fn, true);

  codegen_startfun(c, c_t->serialise_trace_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->serialise_trace_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->serialise_trace_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->serialise_trace_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->serialise_trace_fn, 1);

  LLVMValueRef size = field_value(c, c_t->structure, object, 1);

  LLVMValueRef alloc = LLVMBuildAdd(c->builder, size,
    LLVMConstInt(c->intptr, 1, false), "");

  // Reserve space for the contents.
  LLVMValueRef ptr = field_value(c, c_t->structure, object, 3);

  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = ptr;
  args[2] = alloc;
  gencall_runtime(c, "pony_serialise_reserve", args, 3, "");

  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

void genprim_string_serialise(compile_t* c, reach_type_t* t)
{
  // Generate the serialise function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->serialise_fn = codegen_addfun(c, genname_serialise(t->name),
    c->serialise_fn, true);

  codegen_startfun(c, c_t->serialise_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->serialise_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->serialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->serialise_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->serialise_fn, 1);
  LLVMValueRef addr = LLVMGetParam(c_t->serialise_fn, 2);
  LLVMValueRef offset = LLVMGetParam(c_t->serialise_fn, 3);
  LLVMValueRef mut = LLVMGetParam(c_t->serialise_fn, 4);

  LLVMValueRef offset_addr = LLVMBuildInBoundsGEP2(c->builder, c->i8, addr,
    &offset, 1, "");

  genserialise_serialiseid(c, t, offset_addr);

  // Don't serialise our contents if we are opaque.
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntNE, mut,
    LLVMConstInt(c->i32, PONY_TRACE_OPAQUE, false), "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);
  LLVMPositionBuilderAtEnd(c->builder, body_block);

  // Write the size, and rewrite alloc to be size + 1.
  LLVMValueRef size = field_value(c, c_t->structure, object, 1);
  LLVMValueRef size_loc = field_loc(c, offset_addr, c_t->structure, 1);
  LLVMBuildStore(c->builder, size, size_loc);

  LLVMValueRef alloc = LLVMBuildAdd(c->builder, size,
    LLVMConstInt(c->intptr, 1, false), "");
  LLVMValueRef alloc_loc = field_loc(c, offset_addr, c_t->structure, 2);
  LLVMBuildStore(c->builder, alloc, alloc_loc);

  // Write the pointer.
  LLVMValueRef ptr = field_value(c, c_t->structure, object, 3);

  LLVMValueRef args[5];
  args[0] = ctx;
  args[1] = ptr;
  LLVMValueRef ptr_offset = gencall_runtime(c, "pony_serialise_offset",
    args, 2, "");

  LLVMValueRef ptr_loc = field_loc(c, offset_addr, c_t->structure, 3);
  LLVMBuildStore(c->builder, ptr_offset, ptr_loc);

  // Serialise the string contents.
  LLVMValueRef dst =  LLVMBuildInBoundsGEP2(c->builder, c->i8, addr,
    &ptr_offset, 1, "");
  LLVMValueRef src = field_value(c, c_t->structure, object, 3);
  gencall_memcpy(c, dst, src, alloc);

  LLVMBuildBr(c->builder, post_block);
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

void genprim_string_deserialise(compile_t* c, reach_type_t* t)
{
  // Generate the deserisalise function.
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  c_t->deserialise_fn = codegen_addfun(c, genname_deserialise(t->name),
    c->trace_fn, true);

  codegen_startfun(c, c_t->deserialise_fn, NULL, NULL, NULL, false);
  LLVMSetFunctionCallConv(c_t->deserialise_fn, LLVMCCallConv);
  LLVMSetLinkage(c_t->deserialise_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(c_t->deserialise_fn, 0);
  LLVMValueRef object = LLVMGetParam(c_t->deserialise_fn, 1);

  gendeserialise_serialiseid(c, c_t, object);

  // Deserialise the string contents.
  LLVMValueRef alloc = field_value(c, c_t->structure, object, 2);
  LLVMValueRef ptr_offset = field_value(c, c_t->structure, object, 3);
  ptr_offset = LLVMBuildPtrToInt(c->builder, ptr_offset, c->intptr, "");

  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = ptr_offset;
  args[2] = alloc;
  LLVMValueRef ptr_addr = gencall_runtime(c, "pony_deserialise_block", args, 3,
    "");

  LLVMValueRef ptr = LLVMBuildStructGEP2(c->builder, c_t->structure, object, 3,
    "");
  LLVMBuildStore(c->builder, ptr_addr, ptr);

  genfun_build_ret_void(c);
  codegen_finishfun(c);
}

static void platform_freebsd(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("freebsd", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_freebsd(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_dragonfly(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("dragonfly", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_dragonfly(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_openbsd(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("openbsd", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_openbsd(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_linux(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("linux", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_linux(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_osx(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("osx", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_macosx(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_windows(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("windows", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_windows(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_x86(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("x86", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_x86(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_arm(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("arm", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_arm(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_lp64(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("lp64", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_lp64(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_llp64(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("llp64", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_llp64(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_ilp32(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("ilp32", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_ilp32(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_bigendian(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("bigendian", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_bigendian(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_littleendian(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("littleendian", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_littleendian(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_native128(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("native128", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_native128(c->opt->triple), false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_debug(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("debug", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

  LLVMValueRef result = LLVMConstInt(c->i1, !c->opt->release, false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_runtimestats(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("runtimestats", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

#if defined(USE_RUNTIMESTATS) || defined(USE_RUNTIMESTATS_MESSAGES)
  bool runtimestats_enabled = true;
#else
  bool runtimestats_enabled = false;
#endif

  LLVMValueRef result = LLVMConstInt(c->i1, runtimestats_enabled, false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void platform_runtimestatsmessages(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("runtimestatsmessages", cap);
  start_function(c, t, m, c->i1, &c_t->use_type, 1);

#ifdef USE_RUNTIMESTATS_MESSAGES
  bool runtimestatsmessages_enabled = true;
#else
  bool runtimestatsmessages_enabled = false;
#endif

  LLVMValueRef result = LLVMConstInt(c->i1, runtimestatsmessages_enabled, false);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

void genprim_platform_methods(compile_t* c, reach_type_t* t)
{
  BOX_FUNCTION(platform_freebsd, t);
  BOX_FUNCTION(platform_dragonfly, t);
  BOX_FUNCTION(platform_openbsd, t);
  BOX_FUNCTION(platform_linux, t);
  BOX_FUNCTION(platform_osx, t);
  BOX_FUNCTION(platform_windows, t);
  BOX_FUNCTION(platform_x86, t);
  BOX_FUNCTION(platform_arm, t);
  BOX_FUNCTION(platform_lp64, t);
  BOX_FUNCTION(platform_llp64, t);
  BOX_FUNCTION(platform_ilp32, t);
  BOX_FUNCTION(platform_bigendian, t);
  BOX_FUNCTION(platform_littleendian, t);
  BOX_FUNCTION(platform_native128, t);
  BOX_FUNCTION(platform_debug, t);
  BOX_FUNCTION(platform_runtimestats, t);
  BOX_FUNCTION(platform_runtimestatsmessages, t);
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

static void number_value(compile_t* c, num_conv_t* type, token_id cap)
{
  reach_type_t* t = reach_type_name(c->reach, type->type_name);

  if(t == NULL)
    return;

  FIND_METHOD("_value", cap);
  start_function(c, t, m, type->type, &type->type, 1);

  LLVMValueRef arg = LLVMGetParam(c_m->func, 0);
  genfun_build_ret(c, arg);

  codegen_finishfun(c);
}

static LLVMBasicBlockRef handle_nan(compile_t* c, LLVMValueRef arg,
  LLVMTypeRef int_type, uint64_t exp, uint64_t mantissa)
{
  LLVMBasicBlockRef nan = codegen_block(c, "");
  LLVMBasicBlockRef non_nan = codegen_block(c, "");

  LLVMValueRef exp_mask = LLVMConstInt(int_type, exp, false);
  LLVMValueRef mant_mask = LLVMConstInt(int_type, mantissa, false);

  LLVMValueRef bits = LLVMBuildBitCast(c->builder, arg, int_type, "");
  LLVMValueRef exp_res = LLVMBuildAnd(c->builder, bits, exp_mask, "");
  LLVMValueRef mant_res = LLVMBuildAnd(c->builder, bits, mant_mask, "");

  exp_res = LLVMBuildICmp(c->builder, LLVMIntEQ, exp_res, exp_mask, "");
  mant_res = LLVMBuildICmp(c->builder, LLVMIntNE, mant_res,
    LLVMConstNull(int_type), "");

  LLVMValueRef is_nan = LLVMBuildAnd(c->builder, exp_res, mant_res, "");
  LLVMBuildCondBr(c->builder, is_nan, nan, non_nan);

  LLVMPositionBuilderAtEnd(c->builder, nan);

  return non_nan;
}

static LLVMValueRef handle_overflow_saturate(compile_t* c, LLVMValueRef arg,
  LLVMTypeRef from, LLVMTypeRef to, LLVMValueRef to_max, LLVMValueRef to_min,
  bool sign)
{
  LLVMBasicBlockRef overflow = codegen_block(c, "");
  LLVMBasicBlockRef test_underflow = codegen_block(c, "");
  LLVMBasicBlockRef underflow = codegen_block(c, "");
  LLVMBasicBlockRef normal = codegen_block(c, "");

  LLVMValueRef to_fmax;
  if(sign)
    to_fmax = LLVMBuildSIToFP(c->builder, to_max, from, "");
  else
    to_fmax = LLVMBuildUIToFP(c->builder, to_max, from, "");
  LLVMValueRef is_overflow = LLVMBuildFCmp(c->builder, LLVMRealOGT, arg,
    to_fmax, "");
  LLVMBuildCondBr(c->builder, is_overflow, overflow, test_underflow);

  LLVMPositionBuilderAtEnd(c->builder, overflow);
  genfun_build_ret(c, to_max);

  LLVMPositionBuilderAtEnd(c->builder, test_underflow);

  LLVMValueRef to_fmin;
  if(sign)
    to_fmin = LLVMBuildSIToFP(c->builder, to_min, from, "");
  else
    to_fmin = LLVMBuildUIToFP(c->builder, to_min, from, "");
  LLVMValueRef is_underflow = LLVMBuildFCmp(c->builder, LLVMRealOLT, arg,
    to_fmin, "");
  LLVMBuildCondBr(c->builder, is_underflow, underflow, normal);

  LLVMPositionBuilderAtEnd(c->builder, underflow);
  genfun_build_ret(c, to_min);

  LLVMPositionBuilderAtEnd(c->builder, normal);

  if(sign)
    return LLVMBuildFPToSI(c->builder, arg, to, "");
  return LLVMBuildFPToUI(c->builder, arg, to, "");
}

static LLVMValueRef f32_to_si_saturation(compile_t* c, LLVMValueRef arg,
  num_conv_t* to)
{
  LLVMBasicBlockRef test_overflow = handle_nan(c, arg, c->i32, 0x7F800000,
    0x007FFFFF);
  genfun_build_ret(c, LLVMConstNull(to->type));
  LLVMPositionBuilderAtEnd(c->builder, test_overflow);
  LLVMValueRef to_max = LLVMConstNull(to->type);
  LLVMValueRef to_min = LLVMBuildNot(c->builder, to_max, "");
  to_max = LLVMBuildLShr(c->builder, to_min, LLVMConstInt(to->type, 1, false),
    "");
  to_min = LLVMBuildXor(c->builder, to_max, to_min, "");
  return handle_overflow_saturate(c, arg, c->f32, to->type, to_max, to_min,
    true);
}

static LLVMValueRef f64_to_si_saturation(compile_t* c, LLVMValueRef arg,
  num_conv_t* to)
{
  LLVMBasicBlockRef test_overflow = handle_nan(c, arg, c->i64,
    0x7FF0000000000000, 0x000FFFFFFFFFFFFF);
  genfun_build_ret(c, LLVMConstNull(to->type));
  LLVMPositionBuilderAtEnd(c->builder, test_overflow);
  LLVMValueRef to_max = LLVMConstNull(to->type);
  LLVMValueRef to_min = LLVMBuildNot(c->builder, to_max, "");
  to_max = LLVMBuildLShr(c->builder, to_min, LLVMConstInt(to->type, 1, false),
    "");
  to_min = LLVMBuildXor(c->builder, to_max, to_min, "");
  return handle_overflow_saturate(c, arg, c->f64, to->type, to_max, to_min,
    true);
}

static LLVMValueRef f32_to_ui_saturation(compile_t* c, LLVMValueRef arg,
  num_conv_t* to)
{
  LLVMBasicBlockRef test_overflow = handle_nan(c, arg, c->i32, 0x7F800000,
    0x007FFFFF);
  genfun_build_ret(c, LLVMConstNull(to->type));
  LLVMPositionBuilderAtEnd(c->builder, test_overflow);
  LLVMValueRef to_min = LLVMConstNull(to->type);
  LLVMValueRef to_max = LLVMBuildNot(c->builder, to_min, "");
  return handle_overflow_saturate(c, arg, c->f32, to->type, to_max, to_min,
    false);
}

static LLVMValueRef f32_to_u128_saturation(compile_t* c, LLVMValueRef arg)
{
  LLVMBasicBlockRef test_overflow = handle_nan(c, arg, c->i32, 0x7F800000,
    0x007FFFFF);
  genfun_build_ret(c, LLVMConstNull(c->i128));
  LLVMPositionBuilderAtEnd(c->builder, test_overflow);

  LLVMBasicBlockRef overflow = codegen_block(c, "");
  LLVMBasicBlockRef test_underflow = codegen_block(c, "");
  LLVMBasicBlockRef underflow = codegen_block(c, "");
  LLVMBasicBlockRef normal = codegen_block(c, "");

  LLVMValueRef min = LLVMConstNull(c->f32);
  LLVMValueRef max = LLVMConstInf(c->f32, false);

  LLVMValueRef is_overflow = LLVMBuildFCmp(c->builder, LLVMRealOGE, arg, max,
    "");
  LLVMBuildCondBr(c->builder, is_overflow, overflow, test_underflow);

  LLVMPositionBuilderAtEnd(c->builder, overflow);
  genfun_build_ret(c, LLVMBuildNot(c->builder, LLVMConstNull(c->i128),
    ""));

  LLVMPositionBuilderAtEnd(c->builder, test_underflow);
  LLVMValueRef is_underflow = LLVMBuildFCmp(c->builder, LLVMRealOLT, arg, min,
    "");
  LLVMBuildCondBr(c->builder, is_underflow, underflow, normal);

  LLVMPositionBuilderAtEnd(c->builder, underflow);
  genfun_build_ret(c, LLVMConstNull(c->i128));

  LLVMPositionBuilderAtEnd(c->builder, normal);
  return LLVMBuildFPToUI(c->builder, arg, c->i128, "");
}

static LLVMValueRef f64_to_ui_saturation(compile_t* c, LLVMValueRef arg,
  num_conv_t* to)
{
  LLVMBasicBlockRef test_overflow = handle_nan(c, arg, c->i64,
    0x7FF0000000000000, 0x000FFFFFFFFFFFFF);
  genfun_build_ret(c, LLVMConstNull(to->type));
  LLVMPositionBuilderAtEnd(c->builder, test_overflow);
  LLVMValueRef to_min = LLVMConstNull(to->type);
  LLVMValueRef to_max = LLVMBuildNot(c->builder, to_min, "");
  return handle_overflow_saturate(c, arg, c->f64, to->type, to_max, to_min,
    false);
}

static LLVMValueRef f64_to_f32_saturation(compile_t* c, LLVMValueRef arg)
{
  LLVMBasicBlockRef test_overflow = handle_nan(c, arg, c->i64,
    0x7FF0000000000000, 0x000FFFFFFFFFFFFF);
  genfun_build_ret(c, LLVMConstNaN(c->f32));

  LLVMBasicBlockRef overflow = codegen_block(c, "");
  LLVMBasicBlockRef test_underflow = codegen_block(c, "");
  LLVMBasicBlockRef underflow = codegen_block(c, "");
  LLVMBasicBlockRef normal = codegen_block(c, "");

  LLVMPositionBuilderAtEnd(c->builder, test_overflow);
  LLVMValueRef f32_max = LLVMConstInt(c->i32, 0x7F7FFFFF, false);
  f32_max = LLVMBuildBitCast(c->builder, f32_max, c->f32, "");
  f32_max = LLVMBuildFPExt(c->builder, f32_max, c->f64, "");
  LLVMValueRef is_overflow = LLVMBuildFCmp(c->builder, LLVMRealOGT, arg,
    f32_max, "");
  LLVMBuildCondBr(c->builder, is_overflow, overflow, test_underflow);

  LLVMPositionBuilderAtEnd(c->builder, overflow);
  genfun_build_ret(c, LLVMConstInf(c->f32, false));

  LLVMPositionBuilderAtEnd(c->builder, test_underflow);
  LLVMValueRef f32_min = LLVMConstInt(c->i32, 0xFF7FFFFF, false);
  f32_min = LLVMBuildBitCast(c->builder, f32_min, c->f32, "");
  f32_min = LLVMBuildFPExt(c->builder, f32_min, c->f64, "");
  LLVMValueRef is_underflow = LLVMBuildFCmp(c->builder, LLVMRealOLT, arg,
    f32_min, "");
  LLVMBuildCondBr(c->builder, is_underflow, underflow, normal);

  LLVMPositionBuilderAtEnd(c->builder, underflow);
  genfun_build_ret(c, LLVMConstInf(c->f32, true));

  LLVMPositionBuilderAtEnd(c->builder, normal);
  return LLVMBuildFPTrunc(c->builder, arg, c->f32, "");
}

static LLVMValueRef u128_to_f32_saturation(compile_t* c, LLVMValueRef arg)
{
  LLVMValueRef val_f64 = LLVMBuildUIToFP(c->builder, arg, c->f64, "");
  LLVMValueRef f32_max = LLVMConstInt(c->i32, 0x7F7FFFFF, false);
  f32_max = LLVMBuildBitCast(c->builder, f32_max, c->f32, "");
  f32_max = LLVMBuildFPExt(c->builder, f32_max, c->f64, "");
  LLVMValueRef is_overflow = LLVMBuildFCmp(c->builder, LLVMRealOGT, val_f64,
    f32_max, "");
  LLVMValueRef result = LLVMBuildUIToFP(c->builder, arg, c->f32, "");
  return LLVMBuildSelect(c->builder, is_overflow, LLVMConstInf(c->f32, false),
    result, "");
}

static void number_conversion(compile_t* c, void** data, token_id cap)
{
  num_conv_t* from = (num_conv_t*)data[0];
  num_conv_t* to = (num_conv_t*)data[1];
  bool native128 = data[2] != 0;

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

  FIND_METHOD(to->fun_name, cap);
  start_function(c, t, m, to->type, &from->type, 1);

  LLVMValueRef arg = LLVMGetParam(c_m->func, 0);
  LLVMValueRef result;

  if(from->is_float)
  {
    if(to->is_float)
    {
      if(from->size < to->size)
        result = LLVMBuildFPExt(c->builder, arg, to->type, "");
      else if(from->size > to->size)
        result = f64_to_f32_saturation(c, arg);
      else
        result = arg;
    } else if(to->is_signed) {
      if(from->size < 64)
        result = f32_to_si_saturation(c, arg, to);
      else
        result = f64_to_si_saturation(c, arg, to);
    } else {
      if(from->size < 64)
      {
        if(to->size > 64)
          result = f32_to_u128_saturation(c, arg);
        else
          result = f32_to_ui_saturation(c, arg, to);
      } else
        result = f64_to_ui_saturation(c, arg, to);
    }
  } else if(to->is_float) {
    if(from->is_signed)
      result = LLVMBuildSIToFP(c->builder, arg, to->type, "");
    else if((from->size > 64) && (to->size < 64))
      result = u128_to_f32_saturation(c, arg);
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

  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void unsafe_number_conversion(compile_t* c, void** data, token_id cap)
{
  num_conv_t* from = (num_conv_t*)data[0];
  num_conv_t* to = (num_conv_t*)data[1];
  bool native128 = data[2] != 0;

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

  const char* name = genname_unsafe(to->fun_name);

  FIND_METHOD(name, cap);
  start_function(c, t, m, to->type, &from->type, 1);

  LLVMValueRef arg = LLVMGetParam(c_m->func, 0);
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

  genfun_build_ret(c, result);
  codegen_finishfun(c);
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

  pony_assert(conv != NULL);

  void* data[3];
  data[2] = (void*)target_is_native128(c->opt->triple);

  for(num_conv_t* from = conv; from->type_name != NULL; from++)
  {
    BOX_FUNCTION(number_value, from);
    data[0] = from;
    for(num_conv_t* to = conv; to->type_name != NULL; to++)
    {
      data[1] = to;
      BOX_FUNCTION(number_conversion, data);
      BOX_FUNCTION(unsafe_number_conversion, data);
    }
  }
}

static void f32__nan(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_nan", TK_NONE);
  start_function(c, t, m, c->f32, &c->f32, 1);

  LLVMValueRef result = LLVMConstNaN(c->f32);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void f32__inf(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_inf", TK_NONE);
  LLVMTypeRef params[2];
  params[0] = c->f32;
  params[1] = c->i1;
  start_function(c, t, m, c->f32, params, 2);

  LLVMValueRef sign = LLVMGetParam(c_m->func, 1);
  LLVMValueRef result = LLVMBuildSelect(c->builder, sign,
    LLVMConstInf(c->f32, true), LLVMConstInf(c->f32, false), "");
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void f32_from_bits(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("from_bits", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = c->f32;
  params[1] = c->i32;
  start_function(c, t, m, c->f32, params, 2);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(c_m->func, 1),
    c->f32, "");
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void f32_bits(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("bits", cap);
  start_function(c, t, m, c->i32, &c->f32, 1);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(c_m->func, 0),
    c->i32, "");
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void f64__nan(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_nan", TK_NONE);
  start_function(c, t, m, c->f64, &c->f64, 1);

  LLVMValueRef result = LLVMConstNaN(c->f64);
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void f64__inf(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_inf", TK_NONE);
  LLVMTypeRef params[2];
  params[0] = c->f64;
  params[1] = c->i1;
  start_function(c, t, m, c->f64, params, 2);

  LLVMValueRef sign = LLVMGetParam(c_m->func, 1);
  LLVMValueRef result = LLVMBuildSelect(c->builder, sign,
    LLVMConstInf(c->f64, true), LLVMConstInf(c->f64, false), "");
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void f64_from_bits(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("from_bits", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = c->f64;
  params[1] = c->i64;
  start_function(c, t, m, c->f64, params, 2);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(c_m->func, 1),
    c->f64, "");
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void f64_bits(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("bits", cap);
  start_function(c, t, m, c->i64, &c->f64, 1);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(c_m->func, 0),
    c->i64, "");
  genfun_build_ret(c, result);
  codegen_finishfun(c);
}

static void fp_intrinsics(compile_t* c)
{
  reach_type_t* t;

  if((t = reach_type_name(c->reach, "F32")) != NULL)
  {
    f32__nan(c, t);
    f32__inf(c, t);
    f32_from_bits(c, t);
    BOX_FUNCTION(f32_bits, t);
  }

  if((t = reach_type_name(c->reach, "F64")) != NULL)
  {
    f64__nan(c, t);
    f64__inf(c, t);
    f64_from_bits(c, t);
    BOX_FUNCTION(f64_bits, t);
  }
}

static void make_cpuid(compile_t* c)
{
  if(target_is_x86(c->opt->triple))
  {
    LLVMTypeRef elems[4] = {c->i32, c->i32, c->i32, c->i32};
    LLVMTypeRef r_type = LLVMStructTypeInContext(c->context, elems, 4, false);
    LLVMTypeRef f_type = LLVMFunctionType(r_type, &c->i32, 1, false);
    LLVMValueRef fun = codegen_addfun(c, "internal.x86.cpuid", f_type, false);
    LLVMSetFunctionCallConv(fun, LLVMCCallConv);
    codegen_startfun(c, fun, NULL, NULL, NULL, false);
    LLVMValueRef cpuid = LLVMGetInlineAsm(f_type, "cpuid", 5,
      "={ax},={bx},={cx},={dx},{ax}", 28, false, false, LLVMInlineAsmDialectATT,
      false);
    LLVMValueRef arg = LLVMGetParam(fun, 0);

    LLVMValueRef result = LLVMBuildCall2(c->builder, f_type, cpuid, &arg, 1,
      "");
    genfun_build_ret(c, result);

    codegen_finishfun(c);
  } else {
    (void)c;
  }
}

static void make_rdtscp(compile_t* c)
{
  if(target_is_x86(c->opt->triple))
  {
    // { i64, i32 } @llvm.x86.rdtscp()
    LLVMTypeRef r_type_fields[2] = { c->i64, c->i32 };
    LLVMTypeRef r_type = LLVMStructTypeInContext(c->context, r_type_fields, 2,
      false);
    LLVMTypeRef f_type_r = LLVMFunctionType(r_type, NULL, 0, false);
    LLVMValueRef rdtscp = LLVMAddFunction(c->module, "llvm.x86.rdtscp",
      f_type_r);

    // i64 @internal.x86.rdtscp(i32*)
    LLVMTypeRef ptr_type = c->ptr;
    LLVMTypeRef f_type_f = LLVMFunctionType(c->i64, &ptr_type, 1, false);
    LLVMValueRef fun = codegen_addfun(c, "internal.x86.rdtscp", f_type_f,
      false);
    LLVMSetFunctionCallConv(fun, LLVMCCallConv);

    codegen_startfun(c, fun, NULL, NULL, NULL, false);
    LLVMValueRef result = LLVMBuildCall2(c->builder, f_type_r, rdtscp, NULL, 0,
      "");
    LLVMValueRef second = LLVMBuildExtractValue(c->builder, result, 1, "");
    LLVMValueRef argptr = LLVMGetParam(fun, 0);
    LLVMBuildStore(c->builder, second, argptr);
    LLVMValueRef first = LLVMBuildExtractValue(c->builder, result, 0, "");
    genfun_build_ret(c, first);
    codegen_finishfun(c);
  } else {
    (void)c;
  }
}

static LLVMValueRef make_signature_array(compile_t* c, compile_type_t* c_t,
  const char* signature)
{
  LLVMValueRef args[SIGNATURE_LENGTH];

  for(size_t i = 0; i < SIGNATURE_LENGTH; i++)
    args[i] = LLVMConstInt(c->i8, signature[i], false);

  LLVMValueRef sig = LLVMConstArray(c->i8, args, SIGNATURE_LENGTH);
  LLVMValueRef g_sig = LLVMAddGlobal(c->module, LLVMTypeOf(sig), "");
  LLVMSetLinkage(g_sig, LLVMPrivateLinkage);
  LLVMSetInitializer(g_sig, sig);
  LLVMSetGlobalConstant(g_sig, true);
  LLVMSetUnnamedAddr(g_sig, true);

  args[0] = LLVMConstInt(c->i32, 0, false);
  args[1] = LLVMConstInt(c->i32, 0, false);

  LLVMValueRef ptr = LLVMConstInBoundsGEP2(LLVMTypeOf(sig), g_sig, args, 2);

  args[0] = c_t->desc;
  args[1] = LLVMConstInt(c->intptr, SIGNATURE_LENGTH, false);
  args[2] = args[1];
  args[3] = ptr;

  LLVMValueRef inst = LLVMConstNamedStruct(c_t->structure, args, 4);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, c_t->structure, "");
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);
  LLVMSetLinkage(g_inst, LLVMPrivateLinkage);
  LLVMSetUnnamedAddr(g_inst, true);

  return g_inst;
}

void genprim_signature(compile_t* c)
{
  reach_type_t* t = reach_type_name(c->reach, "Array_U8_val");

  if(t == NULL)
    return;

  compile_type_t* c_t = (compile_type_t*)t->c_type;

  ast_t* def = (ast_t*)ast_data(t->ast);
  pony_assert(def != NULL);

  ast_t* program = ast_nearest(def, TK_PROGRAM);
  pony_assert(program != NULL);

  const char* signature = program_signature(program);
  LLVMValueRef g_array = make_signature_array(c, c_t, signature);

  // Array_U8_val* @internal.signature()
  LLVMTypeRef f_type = LLVMFunctionType(c_t->use_type, NULL, 0, false);
  LLVMValueRef fun = codegen_addfun(c, "internal.signature", f_type, false);
  LLVMSetFunctionCallConv(fun, LLVMCCallConv);
  codegen_startfun(c, fun, NULL, NULL, NULL, false);
  genfun_build_ret(c, g_array);
  codegen_finishfun(c);
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

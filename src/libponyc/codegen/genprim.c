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
#include "../type/cap.h"

#include <assert.h>
#include <string.h>

#define FIND_METHOD(name, cap) \
  const char* strtab_name = stringtab(name); \
  reach_method_t* m = reach_method(t, cap, strtab_name, NULL); \
  if(m == NULL) return; \
  m->intrinsic = true;

#define BOX_FUNCTION(gen, gen_data) \
  box_function(c, (generate_box_fn)gen, gen_data);

#define GENERIC_FUNCTION(name, gen) \
  generic_function(c, t, stringtab(name), gen);

typedef void (*generate_box_fn)(compile_t*, void*, token_id);
typedef void (*generate_gen_fn)(compile_t*, reach_type_t*, reach_method_t*);

static void start_function(compile_t* c, reach_type_t* t, reach_method_t* m,
  LLVMTypeRef result, LLVMTypeRef* params, unsigned count)
{
  m->func_type = LLVMFunctionType(result, params, count, false);
  m->func = codegen_addfun(c, m->full_name, m->func_type);
  genfun_param_attrs(t, m, m->func);
  if(ast_id(ast_childidx(m->r_fun, 5)) != TK_QUESTION)
    LLVMAddFunctionAttr(m->func, LLVMNoUnwindAttribute);
  codegen_startfun(c, m->func, NULL, NULL);
}

static void box_function(compile_t* c, generate_box_fn gen, void* gen_data)
{
  gen(c, gen_data, TK_NONE);
  gen(c, gen_data, TK_REF);
  gen(c, gen_data, TK_VAL);
}

static void generic_function(compile_t* c, reach_type_t* t, const char* name,
  generate_gen_fn gen)
{
  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* mn;
  while((mn = reach_method_names_next(&t->methods, &i)) != NULL)
  {
    if(mn->name == name)
    {
      size_t j = HASHMAP_BEGIN;
      reach_method_t* m;
      while((m = reach_methods_next(&mn->r_methods, &j)) != NULL)
      {
        m->intrinsic = true;
        gen(c, t, m);
      }
      break;
    }
  }
}

static LLVMValueRef field_loc(compile_t* c, LLVMValueRef offset,
  LLVMTypeRef structure, LLVMTypeRef ftype, int index)
{
  LLVMValueRef size = LLVMConstInt(c->intptr,
    LLVMOffsetOfElement(c->target_data, structure, index), false);
  LLVMValueRef f_offset = LLVMBuildInBoundsGEP(c->builder, offset, &size, 1,
    "");

  return LLVMBuildBitCast(c->builder, f_offset,
    LLVMPointerType(ftype, 0), "");
}

static LLVMValueRef field_value(compile_t* c, LLVMValueRef object, int index)
{
  LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, index, "");
  return LLVMBuildLoad(c->builder, field, "");
}

static void pointer_create(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("create", TK_NONE);
  start_function(c, t, m, t->use_type, &t->use_type, 1);

  LLVMValueRef result = LLVMConstNull(t->use_type);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_alloc(compile_t* c, reach_type_t* t,
  reach_type_t* t_elem)
{
  FIND_METHOD("_alloc", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, t, m, t->use_type, params, 2);

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
  FIND_METHOD("_realloc", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, t, m, t->use_type, params, 2);

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
  FIND_METHOD("_unsafe", TK_NONE);
  start_function(c, t, m, t->use_type, &t->use_type, 1);

  LLVMBuildRet(c->builder, LLVMGetParam(m->func, 0));
  codegen_finishfun(c);
}

static void pointer_apply(compile_t* c, void* data, token_id cap)
{
  reach_type_t* t = ((reach_type_t**)data)[0];
  reach_type_t* t_elem = ((reach_type_t**)data)[1];

  FIND_METHOD("_apply", cap);

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, t, m, t_elem->use_type, params, 2);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef index = LLVMGetParam(m->func, 1);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef loc = LLVMBuildInBoundsGEP(c->builder, elem_ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");

  if(cap == TK_VAL)
  {
      LLVMValueRef metadata = LLVMMDNodeInContext(c->context, NULL, 0);
      const char id[] = "invariant.load";
      LLVMSetMetadata(result, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);
  }

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_update(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("_update", TK_NONE);

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = t_elem->use_type;
  start_function(c, t, m, t_elem->use_type, params, 3);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef index = LLVMGetParam(m->func, 1);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef loc = LLVMBuildInBoundsGEP(c->builder, elem_ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");

  LLVMBuildStore(c->builder, LLVMGetParam(m->func, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_offset(compile_t* c, void* data, token_id cap)
{
  reach_type_t* t = ((reach_type_t**)data)[0];
  reach_type_t* t_elem = ((reach_type_t**)data)[1];

  FIND_METHOD("_offset", cap);

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, t, m, t->use_type, params, 2);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);

  // Return ptr + (n * sizeof(len)).
  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef loc = LLVMBuildInBoundsGEP(c->builder, elem_ptr, &n, 1, "");
  LLVMValueRef result = LLVMBuildBitCast(c->builder, loc, t->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_element_size(compile_t* c, reach_type_t* t,
  reach_type_t* t_elem)
{
  FIND_METHOD("_element_size", TK_NONE);
  start_function(c, t, m, c->intptr, &t->use_type, 1);

  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMBuildRet(c->builder, l_size);
  codegen_finishfun(c);
}

static void pointer_insert(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("_insert", TK_NONE);

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, t, m, t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);
  LLVMValueRef len = LLVMGetParam(m->func, 2);

  LLVMValueRef src = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef dst = LLVMBuildInBoundsGEP(c->builder, src, &n, 1, "");
  dst = LLVMBuildBitCast(c->builder, dst, t->use_type, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  // llvm.memmove.*(ptr + (n * sizeof(elem)), ptr, len * sizeof(elem))
  gencall_memmove(c, dst, ptr, elen);

  // Return ptr.
  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);
}

static void pointer_delete(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("_delete", TK_NONE);

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, t, m, t_elem->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);
  LLVMValueRef len = LLVMGetParam(m->func, 2);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, elem_ptr, "");

  LLVMValueRef src = LLVMBuildInBoundsGEP(c->builder, elem_ptr, &n, 1, "");
  src = LLVMBuildBitCast(c->builder, src, t->use_type, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  // llvm.memmove.*(ptr, ptr + (n * sizeof(elem)), len * sizeof(elem))
  gencall_memmove(c, ptr, src, elen);

  // Return ptr[0].
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_copy_to(compile_t* c, void* data, token_id cap)
{
  reach_type_t* t = ((reach_type_t**)data)[0];
  reach_type_t* t_elem = ((reach_type_t**)data)[1];

  FIND_METHOD("_copy_to", cap);

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = t->use_type;
  params[2] = c->intptr;
  start_function(c, t, m, t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(m->func, 1);
  LLVMValueRef n = LLVMGetParam(m->func, 2);
  LLVMValueRef elen = LLVMBuildMul(c->builder, n, l_size, "");

  // llvm.memcpy.*(ptr2, ptr, n * sizeof(elem), 1, 0)
  gencall_memcpy(c, ptr2, ptr, elen);

  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);
}

static void pointer_consume_from(compile_t* c, reach_type_t* t,
  reach_type_t* t_elem)
{
  FIND_METHOD("_consume_from", TK_NONE);

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = t->use_type;
  params[2] = c->intptr;
  start_function(c, t, m, t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(m->func, 1);
  LLVMValueRef n = LLVMGetParam(m->func, 2);
  LLVMValueRef elen = LLVMBuildMul(c->builder, n, l_size, "");

  // llvm.memcpy.*(ptr, ptr2, n * sizeof(elem), 1, 0)
  gencall_memcpy(c, ptr, ptr2, elen);

  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);
}

static void pointer_usize(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("usize", TK_NONE);
  start_function(c, t, m, c->intptr, &t->use_type, 1);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef result = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_is_null(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("is_null", TK_NONE);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, ptr, "");
  LLVMValueRef result = LLVMBuildZExt(c->builder, test, c->ibool, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_eq(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("eq", TK_NONE);
  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = t->use_type;
  start_function(c, t, m, c->ibool, params, 2);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(m->func, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, ptr, ptr2, "");
  LLVMValueRef result = LLVMBuildZExt(c->builder, test, c->ibool, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_lt(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("lt", TK_NONE);
  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = t->use_type;
  start_function(c, t, m, c->ibool, params, 2);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(m->func, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, ptr, ptr2, "");
  LLVMValueRef result = LLVMBuildZExt(c->builder, test, c->ibool, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

void genprim_pointer_methods(compile_t* c, reach_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);

  reach_type_t* box_args[2];
  box_args[0] = t;
  box_args[1] = t_elem;

  pointer_create(c, t);
  pointer_alloc(c, t, t_elem);

  pointer_realloc(c, t, t_elem);
  pointer_unsafe(c, t);
  BOX_FUNCTION(pointer_apply, box_args);
  pointer_update(c, t, t_elem);
  BOX_FUNCTION(pointer_offset, box_args);
  pointer_element_size(c, t, t_elem);
  pointer_insert(c, t, t_elem);
  pointer_delete(c, t, t_elem);
  BOX_FUNCTION(pointer_copy_to, box_args);
  pointer_consume_from(c, t, t_elem);
  pointer_usize(c, t);
  pointer_is_null(c, t);
  pointer_eq(c, t);
  pointer_lt(c, t);
}

static void maybe_create(compile_t* c, reach_type_t* t, reach_type_t* t_elem)
{
  FIND_METHOD("create", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = t_elem->use_type;
  start_function(c, t, m, t->use_type, params, 2);

  LLVMValueRef param = LLVMGetParam(m->func, 1);
  LLVMValueRef result = LLVMBuildBitCast(c->builder, param, t->use_type, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void maybe_none(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("none", TK_NONE);
  start_function(c, t, m, t->use_type, &t->use_type, 1);

  LLVMBuildRet(c->builder, LLVMConstNull(t->use_type));
  codegen_finishfun(c);
}

static void maybe_apply(compile_t* c, void* data, token_id cap)
{
  // Returns the receiver if it isn't null.
  reach_type_t* t = ((reach_type_t**)data)[0];
  reach_type_t* t_elem = ((reach_type_t**)data)[1];

  FIND_METHOD("apply", cap);
  start_function(c, t, m, t_elem->use_type, &t->use_type, 1);

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
}

static void maybe_is_none(compile_t* c, reach_type_t* t, token_id cap)
{
  // Returns true if the receiver is null.
  FIND_METHOD("is_none", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef receiver = LLVMGetParam(m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, receiver, "");
  LLVMValueRef value = LLVMBuildZExt(c->builder, test, c->ibool, "");

  LLVMBuildRet(c->builder, value);
  codegen_finishfun(c);
}

void genprim_maybe_methods(compile_t* c, reach_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);

  reach_type_t* box_args[2];
  box_args[0] = t;
  box_args[1] = t_elem;

  maybe_create(c, t, t_elem);
  maybe_none(c, t);
  BOX_FUNCTION(maybe_apply, box_args);
  BOX_FUNCTION(maybe_is_none, t);
}

static void donotoptimise_apply(compile_t* c, reach_type_t* t,
  reach_method_t* m)
{
  m->intrinsic = true;

  ast_t* typearg = ast_child(m->typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);
  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = t_elem->use_type;

  start_function(c, t, m, m->result->use_type, params, 2);

  LLVMValueRef obj = LLVMGetParam(m->func, 1);
  LLVMTypeRef void_fn = LLVMFunctionType(c->void_type, &t_elem->use_type, 1,
    false);
  LLVMValueRef asmstr = LLVMConstInlineAsm(void_fn, "", "imr,~{memory}", true,
    false);
  LLVMValueRef call = LLVMBuildCall(c->builder, asmstr, &obj, 1, "");
  LLVMAddInstrAttribute(call, 1, LLVMReadOnlyAttribute);
#if PONY_LLVM >= 308
  LLVMSetCallInaccessibleMemOrArgMemOnly(call);
#endif

  LLVMBuildRet(c->builder, m->result->instance);
  codegen_finishfun(c);
}

static void donotoptimise_observe(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("observe", cap);

  start_function(c, t, m, m->result->use_type, &t->use_type, 1);

  LLVMTypeRef void_fn = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMValueRef asmstr = LLVMConstInlineAsm(void_fn, "", "~{memory}", true,
    false);
  LLVMValueRef call = LLVMBuildCall(c->builder, asmstr, NULL, 0, "");
#if PONY_LLVM >= 308
  LLVMSetCallInaccessibleMemOnly(call);
#else
  (void)call;
#endif

  LLVMBuildRet(c->builder, m->result->instance);
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
  LLVMValueRef elem_ptr = LLVMBuildInBoundsGEP(c->builder, pointer, &phi, 1,
    "");
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
  LLVMValueRef offset_addr = LLVMBuildInBoundsGEP(c->builder, addr, &offset, 1,
    "");

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

  LLVMValueRef ptr_offset_addr = LLVMBuildInBoundsGEP(c->builder, addr,
    &ptr_offset, 1, "");

  // Serialise elements.
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reach_type_t* t_elem = reach_type(c->reach, typearg);

  size_t abisize = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, abisize, false);

  if((t_elem->underlying == TK_PRIMITIVE) && (t_elem->primitive != NULL))
  {
    // memcpy machine words
    size = LLVMBuildMul(c->builder, size, l_size, "");
    gencall_memcpy(c, ptr_offset_addr, ptr, size);
  } else {
    ptr = LLVMBuildBitCast(c->builder, ptr,
      LLVMPointerType(t_elem->use_type, 0), "");

    LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
    LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
    LLVMBasicBlockRef body_block = codegen_block(c, "body");
    LLVMBasicBlockRef post_block = codegen_block(c, "post");

    LLVMValueRef offset_var = LLVMBuildAlloca(c->builder, c->void_ptr, "");
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
    LLVMValueRef elem_ptr = LLVMBuildInBoundsGEP(c->builder, ptr, &phi, 1, "");

    ptr_offset_addr = LLVMBuildLoad(c->builder, offset_var, "");
    genserialise_element(c, t_elem, false, ctx, elem_ptr, ptr_offset_addr);
    ptr_offset_addr = LLVMBuildInBoundsGEP(c->builder, ptr_offset_addr, &l_size,
      1, "");
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
    LLVMValueRef elem_ptr = LLVMBuildInBoundsGEP(c->builder, ptr, &phi, 1, "");
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
  LLVMValueRef offset_addr = LLVMBuildInBoundsGEP(c->builder, addr, &offset, 1,
    "");

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
  LLVMValueRef dst =  LLVMBuildInBoundsGEP(c->builder, addr, &ptr_offset, 1,
    "");
  LLVMValueRef src = LLVMBuildBitCast(c->builder, field_value(c, object, 3),
    c->void_ptr, "");
  gencall_memcpy(c, dst, src, alloc);

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

static void platform_freebsd(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("freebsd", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_freebsd(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_linux(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("linux", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_linux(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_osx(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("osx", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_macosx(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_windows(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("windows", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_windows(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_x86(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("x86", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_x86(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_arm(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("arm", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_arm(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_lp64(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("lp64", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_lp64(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_llp64(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("llp64", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_llp64(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_ilp32(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("ilp32", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_ilp32(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_native128(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("native128", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->ibool, target_is_native128(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_debug(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("debug", cap);
  start_function(c, t, m, c->ibool, &t->use_type, 1);

  LLVMValueRef result = LLVMConstInt(c->ibool, !c->opt->release, false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

void genprim_platform_methods(compile_t* c, reach_type_t* t)
{
  BOX_FUNCTION(platform_freebsd, t);
  BOX_FUNCTION(platform_linux, t);
  BOX_FUNCTION(platform_osx, t);
  BOX_FUNCTION(platform_windows, t);
  BOX_FUNCTION(platform_x86, t);
  BOX_FUNCTION(platform_arm, t);
  BOX_FUNCTION(platform_lp64, t);
  BOX_FUNCTION(platform_llp64, t);
  BOX_FUNCTION(platform_ilp32, t);
  BOX_FUNCTION(platform_native128, t);
  BOX_FUNCTION(platform_debug, t);
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

  void* data[3];
  data[2] = (void*)target_is_native128(c->opt->triple);

  for(num_conv_t* from = conv; from->type_name != NULL; from++)
  {
    data[0] = from;
    for(num_conv_t* to = conv; to->type_name != NULL; to++)
    {
      data[1] = to;
      BOX_FUNCTION(number_conversion, data);
    }
  }
}

static void f32__nan(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_nan", TK_NONE);
  start_function(c, t, m, c->f32, &c->f32, 1);

  LLVMValueRef result = LLVMConstNaN(c->f32);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f32_from_bits(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("from_bits", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = c->f32;
  params[1] = c->i32;
  start_function(c, t, m, c->f32, params, 2);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 1),
    c->f32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f32_bits(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("bits", cap);
  start_function(c, t, m, c->i32, &c->f32, 1);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 0),
    c->i32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f64__nan(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("_nan", TK_NONE);
  start_function(c, t, m, c->f64, &c->f64, 1);

  LLVMValueRef result = LLVMConstNaN(c->f64);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f64_from_bits(compile_t* c, reach_type_t* t)
{
  FIND_METHOD("from_bits", TK_NONE);

  LLVMTypeRef params[2];
  params[0] = c->f64;
  params[1] = c->i64;
  start_function(c, t, m, c->f64, params, 2);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 1),
    c->f64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f64_bits(compile_t* c, reach_type_t* t, token_id cap)
{
  FIND_METHOD("bits", cap);
  start_function(c, t, m, c->i64, &c->f64, 1);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 0),
    c->i64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void fp_intrinsics(compile_t* c)
{
  reach_type_t* t;

  if((t = reach_type_name(c->reach, "F32")) != NULL)
  {
    f32__nan(c, t);
    f32_from_bits(c, t);
    BOX_FUNCTION(f32_bits, t);
  }

  if((t = reach_type_name(c->reach, "F64")) != NULL)
  {
    f64__nan(c, t);
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

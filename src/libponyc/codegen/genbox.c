#include "genbox.h"
#include "gencall.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include "gentype.h"
#include "ponyassert.h"

LLVMValueRef gen_box(compile_t* c, ast_t* type, LLVMValueRef value)
{
  LLVMTypeRef l_type = LLVMTypeOf(value);

  if(LLVMGetTypeKind(l_type) == LLVMPointerTypeKind)
    return value;

  reach_type_t* t = reach_type(c->reach, type, c->opt);
  pony_assert(t != NULL);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(l_type != c_t->primitive && l_type != c_t->mem_type)
    return NULL;

  value = gen_assign_cast(c, c_t->mem_type, value, t->ast_cap);

  // Allocate the object.
  LLVMValueRef this_ptr = gencall_allocstruct(c, t);

  // Store the primitive in element 1.
  LLVMValueRef value_ptr = LLVMBuildStructGEP2(c->builder, c_t->structure,
    this_ptr, 1, "");
  LLVMBuildStore(c->builder, value, value_ptr);

  return this_ptr;
}

LLVMValueRef gen_unbox(compile_t* c, ast_t* type, LLVMValueRef object)
{
  LLVMTypeRef l_type = LLVMTypeOf(object);

  if(LLVMGetTypeKind(l_type) != LLVMPointerTypeKind)
    return object;

  reach_type_t* t = reach_type(c->reach, type, c->opt);
  pony_assert(t != NULL);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(c_t->primitive == NULL)
    return object;

  // Extract the primitive type from element 1 and return it.
  LLVMValueRef value_ptr = LLVMBuildStructGEP2(c->builder, c_t->structure,
    object, 1, "");
  LLVMValueRef value = LLVMBuildLoad2(c->builder, c_t->mem_type, value_ptr, "");

  return gen_assign_cast(c, c_t->use_type, value, t->ast_cap);
}

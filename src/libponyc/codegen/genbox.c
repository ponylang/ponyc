#include "genbox.h"
#include "gencall.h"
#include <assert.h>

LLVMValueRef gen_box(compile_t* c, ast_t* type, LLVMValueRef value)
{
  LLVMTypeRef l_type = LLVMTypeOf(value);

  if(LLVMGetTypeKind(l_type) == LLVMPointerTypeKind)
    return value;

  reach_type_t* t = reach_type(c->reach, type);
  assert(t != NULL);

  if(l_type != t->primitive)
    return NULL;

  // Allocate the object.
  LLVMValueRef this_ptr = gencall_allocstruct(c, t);

  // Store the primitive in element 1.
  LLVMValueRef value_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMBuildStore(c->builder, value, value_ptr);

  return this_ptr;
}

LLVMValueRef gen_unbox(compile_t* c, ast_t* type, LLVMValueRef object)
{
  LLVMTypeRef l_type = LLVMTypeOf(object);

  if(LLVMGetTypeKind(l_type) != LLVMPointerTypeKind)
    return object;

  reach_type_t* t = reach_type(c->reach, type);
  assert(t != NULL);

  if(t->primitive == NULL)
    return object;

  // Extract the primitive type from element 1 and return it.
  LLVMValueRef this_ptr = LLVMBuildBitCast(c->builder, object,
    t->structure_ptr, "");
  LLVMValueRef value_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");

  return LLVMBuildLoad(c->builder, value_ptr, "");
}

#include "genbox.h"
#include "gencall.h"

LLVMValueRef gen_box(compile_t* c, ast_t* type, LLVMValueRef value)
{
  LLVMTypeRef l_type = LLVMTypeOf(value);

  if(LLVMGetTypeKind(l_type) == LLVMPointerTypeKind)
    return value;

  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  if(l_type != g.primitive)
    return NULL;

  // Allocate the object as 'this'.
  LLVMValueRef this_ptr = gencall_alloc(c, g.structure_ptr);

  // Set the descriptor.
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 0, "");
  LLVMBuildStore(c->builder, g.desc, desc_ptr);

  // Store the primitive in element 1.
  LLVMValueRef value_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMBuildStore(c->builder, value, value_ptr);

  // Return 'this'.
  return this_ptr;
}

LLVMValueRef gen_unbox(compile_t* c, ast_t* type, LLVMValueRef object)
{
  LLVMTypeRef l_type = LLVMTypeOf(object);

  if(LLVMGetTypeKind(l_type) != LLVMPointerTypeKind)
    return object;

  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  if(g.primitive == NULL)
    return object;

  // Extract the primitive type from element 1 and return it.
  LLVMValueRef this_ptr = LLVMBuildBitCast(c->builder, object, g.structure_ptr,
    "");
  LLVMValueRef value_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");

  return LLVMBuildLoad(c->builder, value_ptr, "");
}

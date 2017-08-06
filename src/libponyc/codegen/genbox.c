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

  reach_type_t* t = reach_type(c->reach, type);
  pony_assert(t != NULL);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(l_type != c_t->primitive)
    return NULL;

  value = gen_assign_cast(c, c_t->mem_type, value, t->ast_cap);

  // Allocate the object.
  LLVMValueRef this_ptr = gencall_allocstruct(c, t);

  // Store the primitive in element 1.
  LLVMValueRef value_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");
  LLVMValueRef store = LLVMBuildStore(c->builder, value, value_ptr);

  const char* box_name = genname_box(t->name);
  LLVMValueRef metadata = tbaa_metadata_for_box_type(c, box_name);
  const char id[] = "tbaa";
  LLVMSetMetadata(store, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);

  return this_ptr;
}

LLVMValueRef gen_unbox(compile_t* c, ast_t* type, LLVMValueRef object)
{
  LLVMTypeRef l_type = LLVMTypeOf(object);

  if(LLVMGetTypeKind(l_type) != LLVMPointerTypeKind)
    return object;

  reach_type_t* t = reach_type(c->reach, type);
  pony_assert(t != NULL);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(c_t->primitive == NULL)
    return object;

  // Extract the primitive type from element 1 and return it.
  LLVMValueRef this_ptr = LLVMBuildBitCast(c->builder, object,
    c_t->structure_ptr, "");
  LLVMValueRef value_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 1, "");

  LLVMValueRef value = LLVMBuildLoad(c->builder, value_ptr, "");

  const char* box_name = genname_box(t->name);
  LLVMValueRef metadata = tbaa_metadata_for_box_type(c, box_name);
  const char id[] = "tbaa";
  LLVMSetMetadata(value, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);

  return gen_assign_cast(c, c_t->use_type, value, t->ast_cap);
}

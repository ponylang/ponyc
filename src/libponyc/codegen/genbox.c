#include "genbox.h"
#include "gencall.h"
#include "genexpr.h"
#include "genfun.h"
#include "genname.h"
#include "gentagged.h"
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

  if(gentagged_is_taggable(c, t))
    return gentagged_box(c, t, value);

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

  if(gentagged_is_taggable(c, t))
  {
    // The pointer may be a tagged value or a heap-boxed value (e.g. from a
    // generic context).
    LLVMValueRef is_tagged = gentagged_is_tagged(c, object);

    LLVMBasicBlockRef tagged_block = codegen_block(c, "unbox_tagged");
    LLVMBasicBlockRef heap_block = codegen_block(c, "unbox_heap");
    LLVMBasicBlockRef merge_block = codegen_block(c, "unbox_merge");
    LLVMBuildCondBr(c->builder, is_tagged, tagged_block, heap_block);

    // Tagged path: extract payload.
    LLVMPositionBuilderAtEnd(c->builder, tagged_block);
    LLVMValueRef tagged_val = gentagged_payload(c, object, c_t->use_type);
    LLVMBuildBr(c->builder, merge_block);
    LLVMBasicBlockRef tagged_from = LLVMGetInsertBlock(c->builder);

    // Heap path: load from struct element 1.
    LLVMMoveBasicBlockAfter(heap_block, tagged_from);
    LLVMPositionBuilderAtEnd(c->builder, heap_block);
    LLVMValueRef value_ptr = LLVMBuildStructGEP2(c->builder, c_t->structure,
      object, 1, "");
    LLVMValueRef heap_val = LLVMBuildLoad2(c->builder, c_t->mem_type,
      value_ptr, "");
    heap_val = gen_assign_cast(c, c_t->use_type, heap_val, t->ast_cap);
    LLVMBuildBr(c->builder, merge_block);
    LLVMBasicBlockRef heap_from = LLVMGetInsertBlock(c->builder);

    // Merge.
    LLVMMoveBasicBlockAfter(merge_block, heap_from);
    LLVMPositionBuilderAtEnd(c->builder, merge_block);
    LLVMValueRef phi = LLVMBuildPhi(c->builder, c_t->use_type, "");
    LLVMValueRef values[2] = {tagged_val, heap_val};
    LLVMBasicBlockRef blocks[2] = {tagged_from, heap_from};
    LLVMAddIncoming(phi, values, blocks, 2);
    return phi;
  }

  // Extract the primitive type from element 1 and return it.
  LLVMValueRef value_ptr = LLVMBuildStructGEP2(c->builder, c_t->structure,
    object, 1, "");
  LLVMValueRef value = LLVMBuildLoad2(c->builder, c_t->mem_type, value_ptr, "");

  return gen_assign_cast(c, c_t->use_type, value, t->ast_cap);
}

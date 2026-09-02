#include "gentagged.h"
#include "gendesc.h"
#include "gentype.h"
#include "ponyassert.h"

#define TAGGED_TAG_BIT   63
#define TAGGED_ID_SHIFT  48
#define TAGGED_ID_MASK   0x7FFF
#define TAGGED_VAL_MASK  0x0000FFFFFFFFFFFFULL

bool gentagged_is_taggable(compile_t* c, reach_type_t* t)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(c_t->primitive == NULL)
    return false;

  // Bare method types use pointer representation regardless of primitive size.
  if(t->bare_method != NULL)
    return false;

  // Only numeric primitive types can be tagged.  Exclude Pointer,
  // NullablePointer (pointer use_type), and tuples (struct use_type).
  LLVMTypeKind use_kind = LLVMGetTypeKind(c_t->use_type);
  if(use_kind != LLVMIntegerTypeKind && use_kind != LLVMFloatTypeKind)
    return false;

  // Only tag on 64-bit targets.
  if(LLVMABISizeOfType(c->target_data, c->intptr) < 8)
    return false;

  // Any numeric primitive fitting in 32 bits is taggable.  On LP64 (Linux,
  // macOS) that covers Bool, U8–U32, I8–I32, and F32.  On LLP64 (Windows)
  // ILong and ULong are also 32-bit and therefore taggable.
  size_t prim_bits = LLVMSizeOfTypeInBits(c->target_data, c_t->primitive);
  return prim_bits <= 32;
}

bool gentagged_has_taggable_subs(compile_t* c, reach_type_t* t)
{
  reach_type_t* sub;
  size_t i = HASHMAP_BEGIN;

  while((sub = reach_type_cache_next(&t->subtypes, &i)) != NULL)
  {
    if(gentagged_is_taggable(c, sub))
      return true;
  }

  return false;
}

bool gentagged_has_any_taggable(compile_t* c)
{
  reach_type_t* t;
  size_t i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(gentagged_is_taggable(c, t))
      return true;
  }

  return false;
}

LLVMValueRef gentagged_box(compile_t* c, reach_type_t* t, LLVMValueRef value)
{
  pony_assert(gentagged_is_taggable(c, t));
  pony_assert(t->type_id <= TAGGED_ID_MASK);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  // Widen the value to the memory representation if needed (e.g. Bool i1->i8).
  if(LLVMTypeOf(value) != c_t->mem_type)
  {
    if(LLVMGetTypeKind(c_t->mem_type) == LLVMIntegerTypeKind &&
       LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMIntegerTypeKind)
    {
      value = LLVMBuildZExt(c->builder, value, c_t->mem_type, "");
    }
    else if(LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMFloatTypeKind)
    {
      value = LLVMBuildBitCast(c->builder, value, c->i32, "");
    }
  }
  else if(LLVMGetTypeKind(c_t->mem_type) == LLVMFloatTypeKind)
  {
    // F32: bitcast to i32 for integer payload encoding.
    value = LLVMBuildBitCast(c->builder, value, c->i32, "");
  }

  // Zero-extend to i64.
  LLVMValueRef payload = LLVMBuildZExt(c->builder, value, c->i64, "");

  // Build the tag: (1 << 63) | (type_id << 48) | payload
  uint64_t tag_const =
    (1ULL << TAGGED_TAG_BIT) |
    ((uint64_t)t->type_id << TAGGED_ID_SHIFT);
  LLVMValueRef tag = LLVMConstInt(c->i64, tag_const, false);
  LLVMValueRef tagged = LLVMBuildOr(c->builder, tag, payload, "");

  // Convert to ptr.
  return LLVMBuildIntToPtr(c->builder, tagged, c->ptr, "");
}

LLVMValueRef gentagged_is_tagged(compile_t* c, LLVMValueRef ptr_value)
{
  LLVMValueRef as_int = LLVMBuildPtrToInt(c->builder, ptr_value, c->i64, "");
  LLVMValueRef shifted = LLVMBuildLShr(c->builder, as_int,
    LLVMConstInt(c->i64, TAGGED_TAG_BIT, false), "");
  return LLVMBuildTrunc(c->builder, shifted, c->i1, "");
}

LLVMValueRef gentagged_typeid(compile_t* c, LLVMValueRef tagged)
{
  LLVMValueRef as_int;

  if(LLVMGetTypeKind(LLVMTypeOf(tagged)) == LLVMPointerTypeKind)
    as_int = LLVMBuildPtrToInt(c->builder, tagged, c->i64, "");
  else
    as_int = tagged;

  LLVMValueRef shifted = LLVMBuildLShr(c->builder, as_int,
    LLVMConstInt(c->i64, TAGGED_ID_SHIFT, false), "");
  LLVMValueRef masked = LLVMBuildAnd(c->builder, shifted,
    LLVMConstInt(c->i64, TAGGED_ID_MASK, false), "");
  return LLVMBuildTrunc(c->builder, masked, c->i32, "");
}

LLVMValueRef gentagged_payload(compile_t* c, LLVMValueRef tagged,
  LLVMTypeRef target_type)
{
  LLVMValueRef as_int;

  if(LLVMGetTypeKind(LLVMTypeOf(tagged)) == LLVMPointerTypeKind)
    as_int = LLVMBuildPtrToInt(c->builder, tagged, c->i64, "");
  else
    as_int = tagged;

  LLVMValueRef payload = LLVMBuildAnd(c->builder, as_int,
    LLVMConstInt(c->i64, TAGGED_VAL_MASK, false), "");

  LLVMTypeKind kind = LLVMGetTypeKind(target_type);

  if(kind == LLVMFloatTypeKind)
  {
    LLVMValueRef truncated = LLVMBuildTrunc(c->builder, payload, c->i32, "");
    return LLVMBuildBitCast(c->builder, truncated, c->f32, "");
  }

  if(kind == LLVMIntegerTypeKind)
  {
    unsigned width = LLVMGetIntTypeWidth(target_type);
    if(width < 64)
      return LLVMBuildTrunc(c->builder, payload, target_type, "");
    return payload;
  }

  pony_assert(0);
  return NULL;
}

static LLVMValueRef gentagged_fetch_desc(compile_t* c, LLVMValueRef tagged,
  reach_type_t* union_type)
{
  LLVMValueRef tid = gentagged_typeid(c, tagged);

  LLVMBasicBlockRef unreachable_block = codegen_block(c, "tagged_unreachable");
  LLVMBasicBlockRef merge_block = codegen_block(c, "tagged_merge_desc");
  LLVMValueRef type_switch = LLVMBuildSwitch(c->builder, tid,
    unreachable_block, 0);

  LLVMPositionBuilderAtEnd(c->builder, merge_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->ptr, "");

  reach_type_t* sub;
  size_t i = HASHMAP_BEGIN;
  unsigned case_count = 0;

  while((sub = reach_type_cache_next(&union_type->subtypes, &i)) != NULL)
  {
    if(!gentagged_is_taggable(c, sub))
      continue;

    compile_type_t* c_sub = (compile_type_t*)sub->c_type;
    LLVMBasicBlockRef case_block = codegen_block(c, "tagged_desc_case");
    LLVMAddCase(type_switch, LLVMConstInt(c->i32, sub->type_id, false),
      case_block);
    LLVMPositionBuilderAtEnd(c->builder, case_block);
    LLVMBuildBr(c->builder, merge_block);
    LLVMAddIncoming(phi, &c_sub->desc, &case_block, 1);
    case_count++;
  }

  pony_assert(case_count > 0);

  LLVMMoveBasicBlockAfter(unreachable_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, unreachable_block);
  LLVMBuildUnreachable(c->builder);

  LLVMMoveBasicBlockAfter(merge_block, unreachable_block);
  LLVMPositionBuilderAtEnd(c->builder, merge_block);
  return phi;
}

static LLVMValueRef gentagged_fetch_desc_all(compile_t* c, LLVMValueRef tagged)
{
  LLVMValueRef tid = gentagged_typeid(c, tagged);

  LLVMBasicBlockRef unreachable_block = codegen_block(c, "tagged_unreachable");
  LLVMBasicBlockRef merge_block = codegen_block(c, "tagged_merge_desc");
  LLVMValueRef type_switch = LLVMBuildSwitch(c->builder, tid,
    unreachable_block, 0);

  LLVMPositionBuilderAtEnd(c->builder, merge_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->ptr, "");

  reach_type_t* sub;
  size_t i = HASHMAP_BEGIN;
  unsigned case_count = 0;

  while((sub = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!gentagged_is_taggable(c, sub))
      continue;

    compile_type_t* c_sub = (compile_type_t*)sub->c_type;
    LLVMBasicBlockRef case_block = codegen_block(c, "tagged_desc_case");
    LLVMAddCase(type_switch, LLVMConstInt(c->i32, sub->type_id, false),
      case_block);
    LLVMPositionBuilderAtEnd(c->builder, case_block);
    LLVMBuildBr(c->builder, merge_block);
    LLVMAddIncoming(phi, &c_sub->desc, &case_block, 1);
    case_count++;
  }

  pony_assert(case_count > 0);

  LLVMMoveBasicBlockAfter(unreachable_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, unreachable_block);
  LLVMBuildUnreachable(c->builder);

  LLVMMoveBasicBlockAfter(merge_block, unreachable_block);
  LLVMPositionBuilderAtEnd(c->builder, merge_block);
  return phi;
}

LLVMValueRef gentagged_fetch_desc_or_heap(compile_t* c, LLVMValueRef value,
  reach_type_t* t, const char* prefix)
{
  if(!gentagged_has_taggable_subs(c, t))
    return gendesc_fetch(c, value);

  LLVMValueRef is_tagged = gentagged_is_tagged(c, value);

  char tagged_name[64];
  char heap_name[64];
  char merge_name[64];
  snprintf(tagged_name, sizeof(tagged_name), "%s_tagged", prefix);
  snprintf(heap_name, sizeof(heap_name), "%s_heap", prefix);
  snprintf(merge_name, sizeof(merge_name), "%s_merge", prefix);

  LLVMBasicBlockRef tagged_block = codegen_block(c, tagged_name);
  LLVMBasicBlockRef heap_block = codegen_block(c, heap_name);
  LLVMBasicBlockRef merge_block = codegen_block(c, merge_name);
  LLVMBuildCondBr(c->builder, is_tagged, tagged_block, heap_block);

  LLVMPositionBuilderAtEnd(c->builder, tagged_block);
  LLVMValueRef tagged_desc = gentagged_fetch_desc(c, value, t);
  LLVMBasicBlockRef tagged_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, merge_block);

  LLVMMoveBasicBlockAfter(heap_block, tagged_from);
  LLVMPositionBuilderAtEnd(c->builder, heap_block);
  LLVMValueRef heap_desc = gendesc_fetch(c, value);
  LLVMBasicBlockRef heap_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, merge_block);

  LLVMMoveBasicBlockAfter(merge_block, heap_from);
  LLVMPositionBuilderAtEnd(c->builder, merge_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->ptr, "");
  LLVMValueRef descs[2] = {tagged_desc, heap_desc};
  LLVMBasicBlockRef blocks[2] = {tagged_from, heap_from};
  LLVMAddIncoming(phi, descs, blocks, 2);
  return phi;
}

LLVMValueRef gentagged_fetch_desc_or_heap_all(compile_t* c,
  LLVMValueRef value, const char* prefix)
{
  if(!gentagged_has_any_taggable(c))
    return gendesc_fetch(c, value);

  LLVMValueRef is_tagged = gentagged_is_tagged(c, value);

  char tagged_name[64];
  char heap_name[64];
  char merge_name[64];
  snprintf(tagged_name, sizeof(tagged_name), "%s_tagged", prefix);
  snprintf(heap_name, sizeof(heap_name), "%s_heap", prefix);
  snprintf(merge_name, sizeof(merge_name), "%s_merge", prefix);

  LLVMBasicBlockRef tagged_block = codegen_block(c, tagged_name);
  LLVMBasicBlockRef heap_block = codegen_block(c, heap_name);
  LLVMBasicBlockRef merge_block = codegen_block(c, merge_name);
  LLVMBuildCondBr(c->builder, is_tagged, tagged_block, heap_block);

  LLVMPositionBuilderAtEnd(c->builder, tagged_block);
  LLVMValueRef tagged_desc = gentagged_fetch_desc_all(c, value);
  LLVMBasicBlockRef tagged_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, merge_block);

  LLVMMoveBasicBlockAfter(heap_block, tagged_from);
  LLVMPositionBuilderAtEnd(c->builder, heap_block);
  LLVMValueRef heap_desc = gendesc_fetch(c, value);
  LLVMBasicBlockRef heap_from = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, merge_block);

  LLVMMoveBasicBlockAfter(merge_block, heap_from);
  LLVMPositionBuilderAtEnd(c->builder, merge_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->ptr, "");
  LLVMValueRef descs[2] = {tagged_desc, heap_desc};
  LLVMBasicBlockRef blocks[2] = {tagged_from, heap_from};
  LLVMAddIncoming(phi, descs, blocks, 2);
  return phi;
}

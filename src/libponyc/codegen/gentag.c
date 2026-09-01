#include "gentag.h"
#include "gendesc.h"
#include "gentype.h"
#include "../reach/reach.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

bool gentag_is_taggable(compile_t* c, reach_type_t* t)
{
  if(c->tag_desc_table == NULL)
    return false;

  compile_type_t* c_t = (compile_type_t*)t->c_type;

  if(c_t->primitive == NULL)
    return false;

  // Only types whose primitive representation is <= 32 bits can be tagged.
  LLVMTypeKind kind = LLVMGetTypeKind(c_t->mem_type);

  if(kind == LLVMFloatTypeKind)
    return true;

  if(kind != LLVMIntegerTypeKind)
    return false;

  return LLVMGetIntTypeWidth(c_t->mem_type) <= 32;
}

LLVMValueRef gentag_is_tagged(compile_t* c, LLVMValueRef ptr)
{
  // Convert pointer to integer, check bit 63.
  LLVMValueRef as_int = LLVMBuildPtrToInt(c->builder, ptr, c->i64, "");
  LLVMValueRef shifted = LLVMBuildLShr(c->builder, as_int,
    LLVMConstInt(c->i64, PONY_TAG_BIT, false), "");
  return LLVMBuildTrunc(c->builder, shifted, c->i1, "is_tagged");
}

LLVMValueRef gentag_box(compile_t* c, reach_type_t* t, LLVMValueRef value)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  pony_assert(gentag_is_taggable(c, t));

  // Widen the value to i32 (zero-extend for integers, bitcast for float).
  LLVMValueRef wide;
  if(LLVMGetTypeKind(c_t->mem_type) == LLVMFloatTypeKind)
  {
    wide = LLVMBuildBitCast(c->builder, value, c->i32, "");
  } else {
    wide = LLVMBuildZExt(c->builder, value, c->i32, "");
  }

  // Build the tagged integer:
  //   (1ULL << 63) | ((uint64_t)type_id << 32) | (uint32_t)value
  LLVMValueRef wide64 = LLVMBuildZExt(c->builder, wide, c->i64, "");
  uint64_t tag_bits = (1ULL << PONY_TAG_BIT) |
    ((uint64_t)t->type_id << PONY_TAG_TYPEID_SHIFT);
  LLVMValueRef tag = LLVMConstInt(c->i64, tag_bits, false);
  LLVMValueRef tagged = LLVMBuildOr(c->builder, tag, wide64, "");

  return LLVMBuildIntToPtr(c->builder, tagged, c->ptr, "tagged");
}

LLVMValueRef gentag_unbox(compile_t* c, reach_type_t* t,
  LLVMValueRef tagged_ptr)
{
  compile_type_t* c_t = (compile_type_t*)t->c_type;
  pony_assert(gentag_is_taggable(c, t));

  // Extract the bottom 32 bits.
  LLVMValueRef as_int = LLVMBuildPtrToInt(c->builder, tagged_ptr, c->i64, "");
  LLVMValueRef masked = LLVMBuildAnd(c->builder, as_int,
    LLVMConstInt(c->i64, PONY_TAG_VALUE_MASK, false), "");
  LLVMValueRef narrow = LLVMBuildTrunc(c->builder, masked, c->i32, "");

  // Narrow to the target type.
  if(LLVMGetTypeKind(c_t->mem_type) == LLVMFloatTypeKind)
  {
    return LLVMBuildBitCast(c->builder, narrow, c->f32, "untagged");
  }

  unsigned bit_width = LLVMGetIntTypeWidth(c_t->mem_type);
  if(bit_width < 32)
    return LLVMBuildTrunc(c->builder, narrow, c_t->mem_type, "untagged");

  // i32 stays as-is (mem_type is i32).
  return narrow;
}

LLVMValueRef gentag_fetch_desc(compile_t* c, LLVMValueRef object)
{
  if(c->tag_desc_table == NULL)
    return gendesc_fetch_raw(c, object);

  // Inline tag check using select — no branches, no PHI issues.
  LLVMValueRef as_int = LLVMBuildPtrToInt(c->builder, object, c->i64, "");
  LLVMValueRef shifted = LLVMBuildLShr(c->builder, as_int,
    LLVMConstInt(c->i64, PONY_TAG_BIT, false), "");
  LLVMValueRef is_tagged = LLVMBuildTrunc(c->builder, shifted, c->i1, "");

  // Speculatively compute table lookup address; unused when not tagged.
  LLVMValueRef shifted2 = LLVMBuildLShr(c->builder, as_int,
    LLVMConstInt(c->i64, PONY_TAG_TYPEID_SHIFT, false), "");
  LLVMValueRef id = LLVMBuildAnd(c->builder, shifted2,
    LLVMConstInt(c->i64, 0x7FFFFFFF, false), "");
  LLVMValueRef type_id = LLVMBuildTrunc(c->builder, id, c->i32, "");
  LLVMValueRef index = LLVMBuildLShr(c->builder, type_id,
    LLVMConstInt(c->i32, 2, false), "");
  LLVMValueRef index_ext = LLVMBuildZExt(c->builder, index, c->intptr, "");
  LLVMValueRef table_addr = LLVMBuildGEP2(c->builder, c->ptr,
    c->tag_desc_table, &index_ext, 1, "");

  // Select: tagged → load from table, not tagged → load from object[0].
  LLVMValueRef addr = LLVMBuildSelect(c->builder, is_tagged, table_addr,
    object, "");
  return LLVMBuildLoad2(c->builder, c->ptr, addr, "");
}

LLVMValueRef gen_tag_desc_table(compile_t* c)
{
  // Tagging requires 64-bit pointers — the scheme encodes the tag bit, type_id,
  // and value in the upper bits of a pointer, which don't exist on ILP32.
  if(LLVMGetIntTypeWidth(c->intptr) < 64)
    return NULL;

  uint32_t len = c->reach->numeric_type_count;
  if(len == 0)
    return NULL;

  size_t size = len * sizeof(LLVMValueRef);
  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(size);
  memset(args, 0, size);

  uint32_t count = 0;
  reach_type_t* t;
  size_t i = HASHMAP_BEGIN;

  while(count < len)
  {
    t = reach_types_next(&c->reach->types, &i);
    pony_assert(t != NULL);

    if(t->is_trait || (t->underlying == TK_STRUCT))
      continue;

    uint32_t type_id = t->type_id;
    if((type_id % 4) == 0)
    {
      compile_type_t* c_t = (compile_type_t*)t->c_type;
      args[type_id >> 2] = c_t->desc;
      count++;
    }
  }

  LLVMTypeRef type = LLVMArrayType(c->ptr, len);
  LLVMValueRef table = LLVMAddGlobal(c->module, type, "__TagDescTable");
  LLVMValueRef value = LLVMConstArray(c->ptr, args, len);
  LLVMSetInitializer(table, value);
  LLVMSetGlobalConstant(table, true);
  LLVMSetAlignment(table, 8);
  LLVMSetLinkage(table, LLVMPrivateLinkage);

  ponyint_pool_free_size(size, args);

  return table;
}

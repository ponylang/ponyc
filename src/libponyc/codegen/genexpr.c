#include "genexpr.h"
#include "genbox.h"
#include "gencall.h"
#include "gencontrol.h"
#include "gendesc.h"
#include "genident.h"
#include "genmatch.h"
#include "genname.h"
#include "genoperator.h"
#include "genopt.h"
#include "genreference.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"

LLVMValueRef gen_expr(compile_t* c, ast_t* ast)
{
  LLVMValueRef ret;
  bool has_scope = ast_has_scope(ast);

  if(has_scope)
    codegen_pushscope(c, ast);

  switch(ast_id(ast))
  {
    case TK_SEQ:
      ret = gen_seq(c, ast);
      break;

    case TK_FVARREF:
    case TK_FLETREF:
      ret = gen_fieldload(c, ast);
      break;

    case TK_TUPLEELEMREF:
    {
      ret = gen_tupleelemptr(c, ast);
      break;
    }

    case TK_EMBEDREF:
      ret = gen_fieldembed(c, ast);
      break;

    case TK_PARAMREF:
      ret = gen_param(c, ast);
      break;

    case TK_VAR:
    case TK_LET:
    case TK_MATCH_CAPTURE:
      ret = gen_localdecl(c, ast);
      break;

    case TK_VARREF:
    case TK_LETREF:
      ret = gen_localload(c, ast);
      break;

    case TK_TYPEREF:
    case TK_DONTCARE:
    case TK_DONTCAREREF:
    case TK_MATCH_DONTCARE:
      ret = GEN_NOTNEEDED;
      break;

    case TK_IF:
      ret = gen_if(c, ast);
      break;

    case TK_IFTYPE_SET:
      ret = gen_iftype(c, ast);
      break;

    case TK_WHILE:
      ret = gen_while(c, ast);
      break;

    case TK_REPEAT:
      ret = gen_repeat(c, ast);
      break;

    case TK_TRY:
    case TK_TRY_NO_CHECK:
      ret = gen_try(c, ast);
      break;

    case TK_MATCH:
      ret = gen_match(c, ast);
      break;

    case TK_CALL:
      ret = gen_call(c, ast);
      break;

    case TK_CONSUME:
    {
      ast_t* ref = ast_childidx(ast, 1);
      ret = gen_expr(c, ref);
      switch(ast_id(ref))
      {
        case TK_LETREF:
        case TK_VARREF:
        {
          const char* name = ast_name(ast_child(ref));
          codegen_local_lifetime_end(c, name);
          break;
        }
        case TK_THIS:
        case TK_PARAMREF:
          break;
        default:
          pony_assert(0);
          break;
      }
      break;
    }

    case TK_RECOVER:
      ret = gen_expr(c, ast_childidx(ast, 1));
      break;

    case TK_BREAK:
      ret = gen_break(c, ast);
      break;

    case TK_CONTINUE:
      ret = gen_continue(c, ast);
      break;

    case TK_RETURN:
      ret = gen_return(c, ast);
      break;

    case TK_ERROR:
      ret = gen_error(c, ast);
      break;

    case TK_IS:
      ret = gen_is(c, ast);
      break;

    case TK_ISNT:
      ret = gen_isnt(c, ast);
      break;

    case TK_ASSIGN:
      ret = gen_assign(c, ast);
      break;

    case TK_THIS:
      ret = gen_this(c, ast);
      break;

    case TK_TRUE:
      ret = LLVMConstInt(c->i1, 1, false);
      break;

    case TK_FALSE:
      ret = LLVMConstInt(c->i1, 0, false);
      break;

    case TK_INT:
      ret = gen_int(c, ast);
      break;

    case TK_FLOAT:
      ret = gen_float(c, ast);
      break;

    case TK_STRING:
      ret = gen_string(c, ast);
      break;

    case TK_TUPLE:
      ret = gen_tuple(c, ast);
      break;

    case TK_FFICALL:
      ret = gen_ffi(c, ast);
      break;

    case TK_ADDRESS:
      ret = gen_addressof(c, ast);
      break;

    case TK_DIGESTOF:
      ret = gen_digestof(c, ast);
      break;

    case TK_COMPILE_INTRINSIC:
      ast_error(c->opt->check.errors, ast, "unimplemented compile intrinsic");
      return NULL;

    case TK_COMPILE_ERROR:
    {
      ast_t* reason_seq = ast_child(ast);
      ast_t* reason = ast_child(reason_seq);
      ast_error(c->opt->check.errors, ast, "compile error: %s",
        ast_name(reason));
      return NULL;
    }

    default:
      ast_error(c->opt->check.errors, ast, "not implemented (codegen unknown)");
      return NULL;
  }

  if(has_scope)
  {
    LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(c->builder);
    LLVMValueRef terminator = LLVMGetBasicBlockTerminator(current_bb);
    if(terminator == NULL)
      codegen_scope_lifetime_end(c);
    codegen_popscope(c);
  }

  return ret;
}

static LLVMValueRef assign_to_tuple(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, ast_t* type)
{
  // Cast each component.
  pony_assert(ast_id(type) == TK_TUPLETYPE);

  int count = LLVMCountStructElementTypes(l_type);
  size_t buf_size = count * sizeof(LLVMTypeRef);
  LLVMTypeRef* elements = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetStructElementTypes(l_type, elements);

  LLVMValueRef result = LLVMGetUndef(l_type);

  ast_t* type_child = ast_child(type);
  int i = 0;

  while(type_child != NULL)
  {
    LLVMValueRef r_child = LLVMBuildExtractValue(c->builder, r_value, i, "");
    LLVMValueRef cast_value = gen_assign_cast(c, elements[i], r_child,
      type_child);

    if(cast_value == NULL)
    {
      ponyint_pool_free_size(buf_size, elements);
      return NULL;
    }

    result = LLVMBuildInsertValue(c->builder, result, cast_value, i, "");
    type_child = ast_sibling(type_child);
    i++;
  }

  ponyint_pool_free_size(buf_size, elements);
  return result;
}

static LLVMValueRef assign_ptr_to_tuple(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, LLVMValueRef r_desc);

static LLVMValueRef assign_ptr_to_tuple_element_object(compile_t* c,
  LLVMTypeRef l_field_type, LLVMValueRef r_field)
{
  switch(LLVMGetTypeKind(l_field_type))
  {
    case LLVMIntegerTypeKind:
    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
    {
      LLVMValueRef offset = LLVMConstInt(c->i32,
        target_is_ilp32(c->opt->triple) ? 4 : 8, false);
      r_field = LLVMBuildBitCast(c->builder, r_field, c->void_ptr, "");
      r_field = LLVMBuildInBoundsGEP(c->builder, r_field, &offset, 1, "");
      r_field = LLVMBuildBitCast(c->builder, r_field,
        LLVMPointerType(l_field_type, 0), "");
      return LLVMBuildLoad(c->builder, r_field, "");
    }

    case LLVMStructTypeKind:
    {
      return assign_ptr_to_tuple(c, l_field_type, r_field, NULL);
    }

    case LLVMPointerTypeKind:
    {
      return LLVMBuildBitCast(c->builder, r_field, l_field_type, "");
    }

    default: {}
  }

  pony_assert(0);
  return NULL;
}

static LLVMValueRef assign_ptr_to_tuple_element_box(compile_t* c,
  LLVMValueRef r_field_ptr, LLVMValueRef r_field_size,
  LLVMValueRef r_field_desc)
{
  LLVMBasicBlockRef box_block = codegen_block(c, "assign_ptr_tuple_box");
  LLVMBasicBlockRef nonbox_block = codegen_block(c, "assign_ptr_tuple_nonbox");
  LLVMBasicBlockRef continue_block = codegen_block(c,
    "assign_ptr_tuple_merge_box");
  LLVMValueRef boxed_mask = LLVMConstInt(c->i32, 1, false);

  LLVMValueRef r_typeid = gendesc_typeid(c, r_field_desc);
  LLVMValueRef boxed = LLVMBuildAnd(c->builder, r_typeid, boxed_mask, "");
  boxed = LLVMBuildICmp(c->builder, LLVMIntEQ, boxed,
    LLVMConstInt(c->i32, 0, false), "");
  LLVMBuildCondBr(c->builder, boxed, box_block, nonbox_block);

  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->object_ptr, "");

  LLVMPositionBuilderAtEnd(c->builder, box_block);
  LLVMValueRef object = gen_box_ptr(c, r_field_ptr, r_field_size, r_field_desc);
  LLVMBuildBr(c->builder, continue_block);
  LLVMAddIncoming(phi, &object, &box_block, 1);

  LLVMPositionBuilderAtEnd(c->builder, nonbox_block);
  LLVMValueRef object_ptr = LLVMBuildBitCast(c->builder, r_field_ptr,
    LLVMPointerType(c->object_ptr, 0), "");
  object = LLVMBuildLoad(c->builder, object_ptr, "");
  LLVMBuildBr(c->builder, continue_block);
  LLVMAddIncoming(phi, &object, &nonbox_block, 1);

  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  return phi;
}

static LLVMValueRef assign_ptr_to_tuple_element_ptr(compile_t* c,
  LLVMTypeRef l_field_type, LLVMValueRef r_field_ptr, LLVMValueRef r_field_size,
  LLVMValueRef r_field_desc)
{
  switch(LLVMGetTypeKind(l_field_type))
  {
    case LLVMIntegerTypeKind:
    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
      r_field_ptr = LLVMBuildBitCast(c->builder, r_field_ptr,
        LLVMPointerType(l_field_type, 0), "");
      return LLVMBuildLoad(c->builder, r_field_ptr, "");

    case LLVMStructTypeKind:
      return assign_ptr_to_tuple(c, l_field_type, r_field_ptr, r_field_desc);

    case LLVMPointerTypeKind:
      return assign_ptr_to_tuple_element_box(c, r_field_ptr, r_field_size,
        r_field_desc);

    default: {}
  }

  pony_assert(0);
  return NULL;
}

static LLVMValueRef assign_ptr_to_tuple_element(compile_t* c,
  LLVMValueRef l_value, LLVMTypeRef l_field_type, LLVMValueRef r_fields,
  LLVMValueRef r_desc, unsigned int field_index)
{
  LLVMValueRef r_field_info = gendesc_fieldinfo(c, r_desc, field_index);
  LLVMValueRef r_field_ptr = gendesc_fieldptr(c, r_fields, r_field_info);
  LLVMValueRef r_field_size = gendesc_fieldsize(c, r_field_info);
  LLVMValueRef r_field_desc = gendesc_fielddesc(c, r_field_info);

  LLVMTypeRef obj_ptr_ptr = LLVMPointerType(c->object_ptr, 0);
  r_field_ptr = LLVMBuildBitCast(c->builder, r_field_ptr,
    obj_ptr_ptr, "");

  LLVMBasicBlockRef null_block = codegen_block(c,
    "assign_ptr_tuple_null_desc");
  LLVMBasicBlockRef nonnull_block = codegen_block(c,
    "assign_ptr_tuple_nonnull_desc");
  LLVMBasicBlockRef continue_block = codegen_block(c,
    "assign_ptr_tuple_merge_desc");
  LLVMValueRef test = LLVMBuildIsNull(c->builder, r_field_desc, "");
  LLVMBuildCondBr(c->builder, test, null_block, nonnull_block);

  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, l_field_type, "");

  LLVMPositionBuilderAtEnd(c->builder, null_block);
  LLVMValueRef r_field = LLVMBuildLoad(c->builder, r_field_ptr, "");
  LLVMValueRef l_field = assign_ptr_to_tuple_element_object(c, l_field_type,
    r_field);
  LLVMBuildBr(c->builder, continue_block);
  LLVMBasicBlockRef cur_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &l_field, &cur_block, 1);

  LLVMMoveBasicBlockAfter(nonnull_block, cur_block);
  LLVMPositionBuilderAtEnd(c->builder, nonnull_block);
  l_field = assign_ptr_to_tuple_element_ptr(c, l_field_type, r_field_ptr,
    r_field_size, r_field_desc);
  LLVMBuildBr(c->builder, continue_block);
  cur_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &l_field, &cur_block, 1);

  LLVMMoveBasicBlockAfter(continue_block, cur_block);
  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  l_value = LLVMBuildInsertValue(c->builder, l_value, phi, field_index, "");

  return l_value;
}

static LLVMValueRef assign_ptr_to_tuple(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, LLVMValueRef r_desc)
{
  unsigned int count = LLVMCountStructElementTypes(l_type);
  size_t buf_size = count * sizeof(LLVMTypeRef);
  LLVMTypeRef* elements = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetStructElementTypes(l_type, elements);

  LLVMValueRef result = LLVMGetUndef(l_type);
  LLVMValueRef r_fields;

  if(r_desc == NULL)
  {
    r_desc = gendesc_fetch(c, r_value);
    r_fields = gendesc_ptr_to_fields(c, r_value, r_desc);
  } else {
    r_fields = LLVMBuildBitCast(c->builder, r_value, c->void_ptr, "");
  }

  for(unsigned int i = 0; i < count; i++)
  {
    result = assign_ptr_to_tuple_element(c, result, elements[i], r_fields,
      r_desc, i);
  }

  ponyint_pool_free_size(buf_size, elements);

  return result;
}

LLVMValueRef gen_assign_cast(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, ast_t* type)
{
  if(r_value <= GEN_NOVALUE)
    return r_value;

  pony_assert(r_value != GEN_NOTNEEDED);

  LLVMTypeRef r_type = LLVMTypeOf(r_value);

  if(r_type == l_type)
    return r_value;

  switch(LLVMGetTypeKind(l_type))
  {
    case LLVMIntegerTypeKind:
      // This can occur for Bool as its value type differs from its memory
      // representation type.
      if(LLVMGetTypeKind(r_type) == LLVMIntegerTypeKind)
      {
        unsigned l_width = LLVMGetIntTypeWidth(l_type);
        unsigned r_width = LLVMGetIntTypeWidth(r_type);

        if(l_width > r_width)
          return LLVMBuildZExt(c->builder, r_value, l_type, "");
        else
          return LLVMBuildTrunc(c->builder, r_value, l_type, "");
      }
      // fallthrough

    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
      pony_assert(LLVMGetTypeKind(r_type) == LLVMPointerTypeKind);
      r_value = gen_unbox(c, type, r_value);
      // The value could be needed in either its value representation type or
      // its memory representation type. Cast the unboxed value to ensure the
      // final type is correct.
      return gen_assign_cast(c, l_type, r_value, type);

    case LLVMPointerTypeKind:
      r_value = gen_box(c, type, r_value);

      if(r_value == NULL)
        return NULL;

      return LLVMBuildBitCast(c->builder, r_value, l_type, "");

    case LLVMStructTypeKind:
      if(LLVMGetTypeKind(r_type) == LLVMPointerTypeKind)
      {
        if(ast_id(type) == TK_TUPLETYPE)
          r_value = gen_unbox(c, type, r_value);
        else
          return assign_ptr_to_tuple(c, l_type, r_value, NULL);

        pony_assert(LLVMGetTypeKind(LLVMTypeOf(r_value)) == LLVMStructTypeKind);
      }

      return assign_to_tuple(c, l_type, r_value, type);

    default: {}
  }

  pony_assert(0);
  return NULL;
}

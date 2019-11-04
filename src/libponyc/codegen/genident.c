#include "genident.h"
#include "genbox.h"
#include "gencall.h"
#include "gendesc.h"
#include "genexpr.h"
#include "genfun.h"
#include "genopt.h"
#include "../reach/subtype.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

static LLVMValueRef tuple_is_box(compile_t* c, ast_t* left_type,
  reach_type_t* right_type, LLVMValueRef l_value, LLVMValueRef r_value,
  LLVMValueRef r_desc, bool rhs_boxed, bool check_cardinality);

static LLVMValueRef gen_is_value(compile_t* c, ast_t* left_type,
  ast_t* right_type, LLVMValueRef l_value, LLVMValueRef r_value);

static LLVMValueRef tuple_is(compile_t* c, ast_t* left_type, ast_t* right_type,
  LLVMValueRef l_value, LLVMValueRef r_value)
{
  pony_assert(ast_id(left_type) == TK_TUPLETYPE);
  pony_assert(ast_id(right_type) == TK_TUPLETYPE);
  pony_assert(ast_childcount(left_type) == ast_childcount(right_type));

  // Pairwise comparison.
  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  // Set up the phi node.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");

  ast_t* left_child = ast_child(left_type);
  ast_t* right_child = ast_child(right_type);
  int i = 0;

  while(left_child != NULL)
  {
    // Set up the next block.
    LLVMBasicBlockRef next_block = codegen_block(c, "next");
    LLVMPositionBuilderAtEnd(c->builder, this_block);

    // Test the element.
    compile_type_t* c_t_left =
      (compile_type_t*)reach_type(c->reach, left_child)->c_type;
    compile_type_t* c_t_right =
      (compile_type_t*)reach_type(c->reach, right_child)->c_type;
    LLVMValueRef l_elem = LLVMBuildExtractValue(c->builder, l_value, i, "");
    LLVMValueRef r_elem = LLVMBuildExtractValue(c->builder, r_value, i, "");
    l_elem = gen_assign_cast(c, c_t_left->use_type, l_elem, left_child);
    r_elem = gen_assign_cast(c, c_t_right->use_type, r_elem, right_child);
    LLVMValueRef test = gen_is_value(c, left_child, right_child, l_elem,
      r_elem);

    // If false, go directly to the post block.
    LLVMBuildCondBr(c->builder, test, next_block, post_block);
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &test, &current_block, 1);

    // Point to the next block.
    LLVMMoveBasicBlockAfter(next_block, LLVMGetInsertBlock(c->builder));
    this_block = next_block;

    left_child = ast_sibling(left_child);
    right_child = ast_sibling(right_child);
    i++;
  }

  // The last block is reached if every element returns true. Jump directly to
  // the post block.
  LLVMPositionBuilderAtEnd(c->builder, this_block);
  LLVMBuildBr(c->builder, post_block);

  LLVMMoveBasicBlockAfter(post_block, this_block);
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef one = LLVMConstInt(c->i1, 1, false);
  LLVMAddIncoming(phi, &one, &this_block, 1);

  return phi;
}

static LLVMValueRef raw_is_box(compile_t* c, ast_t* left_type,
  LLVMValueRef l_value, LLVMValueRef r_value)
{
  pony_assert(LLVMGetTypeKind(LLVMTypeOf(r_value)) == LLVMPointerTypeKind);

  LLVMValueRef r_desc = gendesc_fetch(c, r_value);
  LLVMValueRef same_type = gendesc_isentity(c, r_desc, left_type);
  pony_assert(same_type != GEN_NOVALUE);

  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef value_block = codegen_block(c, "is_value");
  LLVMBasicBlockRef post_block = codegen_block(c, "is_post");
  LLVMBuildCondBr(c->builder, same_type, value_block, post_block);

  LLVMPositionBuilderAtEnd(c->builder, value_block);
  r_value = gen_unbox(c, left_type, r_value);
  LLVMValueRef is_value = gen_is_value(c, left_type, left_type, l_value,
    r_value);
  LLVMBuildBr(c->builder, post_block);
  value_block = LLVMGetInsertBlock(c->builder);

  LLVMMoveBasicBlockAfter(post_block, value_block);
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");
  LLVMValueRef zero = LLVMConstInt(c->i1, 0, false);
  LLVMAddIncoming(phi, &is_value, &value_block, 1);
  LLVMAddIncoming(phi, &zero, &this_block, 1);
  return phi;
}

static LLVMValueRef tuple_element_is_box_unboxed_element(compile_t* c,
  ast_t* l_field_type, LLVMValueRef l_field, LLVMValueRef r_field,
  LLVMValueRef r_field_desc, int field_kind)
{
  LLVMValueRef zero = LLVMConstInt(c->i1, 0, false);

  LLVMBasicBlockRef num_block = codegen_block(c, "is_tuple_num");
  LLVMBasicBlockRef bothnum_block = NULL;

  if(field_kind == SUBTYPE_KIND_NUMERIC)
    bothnum_block = codegen_block(c, "is_tuple_bothnum");

  LLVMBasicBlockRef tuple_block = codegen_block(c, "is_tuple_tuple");
  LLVMBasicBlockRef nonbox_block = codegen_block(c, "is_tuple_nonbox");
  LLVMBasicBlockRef post_block = codegen_block(c, "is_tuple_post");
  LLVMValueRef boxed_mask = LLVMConstInt(c->i32, 3, false);

  LLVMValueRef r_field_typeid = gendesc_typeid(c, r_field_desc);
  LLVMValueRef boxed = LLVMBuildAnd(c->builder, r_field_typeid, boxed_mask,
    "");
  LLVMValueRef box_switch = LLVMBuildSwitch(c->builder, boxed, nonbox_block, 0);
  LLVMAddCase(box_switch, LLVMConstInt(c->i32, 0, false), num_block);
  LLVMAddCase(box_switch, LLVMConstInt(c->i32, 2, false), tuple_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");

  LLVMPositionBuilderAtEnd(c->builder, num_block);

  switch(field_kind)
  {
    case SUBTYPE_KIND_NUMERIC:
    {
      compile_type_t* c_t_left =
        (compile_type_t*)reach_type(c->reach, l_field_type)->c_type;
      LLVMValueRef l_desc = LLVMBuildBitCast(c->builder, c_t_left->desc,
        c->descriptor_ptr, "");
      LLVMValueRef same_type = LLVMBuildICmp(c->builder, LLVMIntEQ, l_desc,
        r_field_desc, "");
      LLVMBuildCondBr(c->builder, same_type, bothnum_block, post_block);
      LLVMAddIncoming(phi, &zero, &num_block, 1);

      LLVMPositionBuilderAtEnd(c->builder, bothnum_block);
      LLVMValueRef r_ptr = LLVMBuildBitCast(c->builder, r_field,
        LLVMPointerType(c_t_left->mem_type, 0), "");
      LLVMValueRef r_value = LLVMBuildLoad(c->builder, r_ptr, "");
      r_value = gen_assign_cast(c, c_t_left->use_type, r_value, l_field_type);
      LLVMValueRef same_identity = gen_is_value(c, l_field_type, l_field_type,
        l_field, r_value);
      LLVMBuildBr(c->builder, post_block);
      pony_assert(LLVMGetInsertBlock(c->builder) == bothnum_block);
      LLVMAddIncoming(phi, &same_identity, &bothnum_block, 1);
      break;
    }

    default:
    {
      LLVMBuildBr(c->builder, post_block);
      LLVMAddIncoming(phi, &zero, &num_block, 1);
      break;
    }
  }

  LLVMPositionBuilderAtEnd(c->builder, tuple_block);

  switch(field_kind)
  {
    case SUBTYPE_KIND_TUPLE:
    {
      LLVMValueRef same_identity = tuple_is_box(c, l_field_type, NULL, l_field,
        r_field, r_field_desc, false, true);
      LLVMBuildBr(c->builder, post_block);
      LLVMBasicBlockRef cur_block = LLVMGetInsertBlock(c->builder);
      LLVMAddIncoming(phi, &same_identity, &cur_block, 1);
      break;
    }

    default:
    {
      LLVMBuildBr(c->builder, post_block);
      LLVMAddIncoming(phi, &zero, &tuple_block, 1);
      break;
    }
  }

  LLVMMoveBasicBlockAfter(nonbox_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, nonbox_block);

  switch(field_kind)
  {
    case SUBTYPE_KIND_UNBOXED:
    {
      LLVMValueRef l_object = LLVMBuildBitCast(c->builder, l_field,
        c->object_ptr, "");
      LLVMValueRef r_object = LLVMBuildLoad(c->builder, r_field, "");
      LLVMValueRef same_identity = LLVMBuildICmp(c->builder, LLVMIntEQ,
        l_object, r_object, "");
      LLVMBuildBr(c->builder, post_block);
      LLVMAddIncoming(phi, &same_identity, &nonbox_block, 1);
      break;
    }

    default:
    {
      LLVMBuildBr(c->builder, post_block);
      LLVMAddIncoming(phi, &zero, &nonbox_block, 1);
      break;
    }
  }

  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  return phi;
}

static LLVMValueRef tuple_is_box_element(compile_t* c, ast_t* l_field_type,
  LLVMValueRef l_field, LLVMValueRef r_fields, LLVMValueRef r_desc,
  unsigned int field_index)
{
  int field_kind = SUBTYPE_KIND_UNBOXED;

  if((ast_id(l_field_type) == TK_TUPLETYPE))
    field_kind = SUBTYPE_KIND_TUPLE;
  else if(is_machine_word(l_field_type))
    field_kind = SUBTYPE_KIND_NUMERIC;

  LLVMValueRef r_field_info = gendesc_fieldinfo(c, r_desc, field_index);
  LLVMValueRef r_field_ptr = gendesc_fieldptr(c, r_fields, r_field_info);
  LLVMValueRef r_field_desc = gendesc_fielddesc(c, r_field_info);

  LLVMTypeRef obj_ptr_ptr = LLVMPointerType(c->object_ptr, 0);
  r_field_ptr = LLVMBuildBitCast(c->builder, r_field_ptr,
    obj_ptr_ptr, "");

  LLVMBasicBlockRef null_block = codegen_block(c, "is_tuple_null_desc");
  LLVMBasicBlockRef nonnull_block = codegen_block(c, "is_tuple_nonnull_desc");
  LLVMBasicBlockRef continue_block = codegen_block(c, "is_tuple_merge_desc");
  LLVMValueRef test = LLVMBuildIsNull(c->builder, r_field_desc, "");
  LLVMBuildCondBr(c->builder, test, null_block, nonnull_block);

  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");

  LLVMPositionBuilderAtEnd(c->builder, null_block);
  LLVMValueRef r_field = LLVMBuildLoad(c->builder, r_field_ptr, "");
  LLVMValueRef field_test;

  switch(field_kind)
  {
    case SUBTYPE_KIND_NUMERIC:
      field_test = raw_is_box(c, l_field_type, l_field, r_field);
      break;

    case SUBTYPE_KIND_TUPLE:
      field_test = tuple_is_box(c, l_field_type, NULL, l_field, r_field, NULL,
        true, true);
      break;

    case SUBTYPE_KIND_UNBOXED:
      field_test = gen_is_value(c, l_field_type, NULL, l_field, r_field);
      break;

    default: {}
  }

  LLVMBuildBr(c->builder, continue_block);
  LLVMBasicBlockRef cur_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &field_test, &cur_block, 1);

  LLVMMoveBasicBlockAfter(nonnull_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, nonnull_block);

  field_test = tuple_element_is_box_unboxed_element(c, l_field_type, l_field,
    r_field_ptr, r_field_desc, field_kind);
  LLVMBuildBr(c->builder, continue_block);
  cur_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &field_test, &cur_block, 1);

  LLVMMoveBasicBlockAfter(continue_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, continue_block);
  return phi;
}

static LLVMValueRef tuple_is_box(compile_t* c, ast_t* left_type,
  reach_type_t* right_type, LLVMValueRef l_value, LLVMValueRef r_value,
  LLVMValueRef r_desc, bool rhs_boxed, bool check_cardinality)
{
  pony_assert(LLVMGetTypeKind(LLVMTypeOf(l_value)) == LLVMStructTypeKind);
  pony_assert(LLVMGetTypeKind(LLVMTypeOf(r_value)) == LLVMPointerTypeKind);

  size_t cardinality = ast_childcount(left_type);

  // If check_cardinality is false, trust the caller and don't do the check.
  // If check_cardinality is true, we can avoid the check if every possible
  // subtype is a tuple and has the correct cardinality.
  if(check_cardinality && (right_type != NULL))
  {
    check_cardinality = false;

    reach_type_t* r_sub;
    size_t i = HASHMAP_BEGIN;

    while((r_sub = reach_type_cache_next(&right_type->subtypes, &i)) != NULL)
    {
      if((ast_id(r_sub->ast) != TK_TUPLETYPE) ||
        (ast_childcount(r_sub->ast) != cardinality))
      {
        check_cardinality = true;
        break;
      }
    }
  }

  LLVMValueRef zero = LLVMConstInt(c->i1, 0, false);
  LLVMValueRef one = LLVMConstInt(c->i1, 1, false);

  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef card_block = NULL;

  if(check_cardinality)
    card_block = codegen_block(c, "is_tuple_card");

  LLVMBasicBlockRef post_block = codegen_block(c, "is_tuple_post");

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");

  LLVMPositionBuilderAtEnd(c->builder, this_block);

  // If rhs_boxed is false, we're handling an unboxed tuple nested as a field
  // in a boxed tuple.

  if(r_desc == NULL)
  {
    pony_assert(rhs_boxed);
    r_desc = gendesc_fetch(c, r_value);
  }

  if(card_block != NULL)
  {
    LLVMValueRef l_card = LLVMConstInt(c->i32, cardinality, false);
    LLVMValueRef r_card = gendesc_fieldcount(c, r_desc);
    LLVMValueRef card_test = LLVMBuildICmp(c->builder, LLVMIntEQ, l_card,
      r_card, "");
    LLVMBuildCondBr(c->builder, card_test, card_block, post_block);
    LLVMAddIncoming(phi, &zero, &this_block, 1);

    LLVMPositionBuilderAtEnd(c->builder, card_block);
  }

  LLVMValueRef r_fields;

  if(rhs_boxed)
    r_fields = gendesc_ptr_to_fields(c, r_value, r_desc);
  else
    r_fields = LLVMBuildBitCast(c->builder, r_value, c->void_ptr, "");

  ast_t* l_child = ast_child(left_type);
  unsigned int i = 0;

  LLVMBasicBlockRef next_block = NULL;

  while(l_child != NULL)
  {
    reach_type_t* t = reach_type(c->reach, l_child);
    compile_type_t* c_t = (compile_type_t*)t->c_type;

    LLVMValueRef l_elem = LLVMBuildExtractValue(c->builder, l_value, i, "");
    l_elem = gen_assign_cast(c, c_t->use_type, l_elem, l_child);
    LLVMValueRef elem_test = tuple_is_box_element(c, l_child, l_elem, r_fields,
      r_desc, i);
    next_block = codegen_block(c, "is_tuple_next");
    LLVMBuildCondBr(c->builder, elem_test, next_block, post_block);
    LLVMBasicBlockRef cur_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &zero, &cur_block, 1);

    LLVMPositionBuilderAtEnd(c->builder, next_block);

    l_child = ast_sibling(l_child);
    i++;
  }

  LLVMBuildBr(c->builder, post_block);
  LLVMAddIncoming(phi, &one, &next_block, 1);

  LLVMMoveBasicBlockAfter(post_block, next_block);
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  return phi;
}

static LLVMValueRef box_is_box(compile_t* c, reach_type_t* left_type,
  LLVMValueRef l_value, LLVMValueRef r_value, int sub_kind)
{
  pony_assert(LLVMGetTypeKind(LLVMTypeOf(l_value)) == LLVMPointerTypeKind);
  pony_assert(LLVMGetTypeKind(LLVMTypeOf(r_value)) == LLVMPointerTypeKind);

  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef box_block = codegen_block(c, "is_box");
  LLVMBasicBlockRef num_block = NULL;
  LLVMBasicBlockRef bothnum_block = NULL;
  LLVMBasicBlockRef tuple_block = NULL;
  LLVMBasicBlockRef bothtuple_block = NULL;

  if((sub_kind & SUBTYPE_KIND_NUMERIC) != 0)
  {
    num_block = codegen_block(c, "is_num");
    bothnum_block = codegen_block(c, "is_bothnum");
  }

  if((sub_kind & SUBTYPE_KIND_TUPLE) != 0)
  {
    tuple_block = codegen_block(c, "is_tuple");
    bothtuple_block = codegen_block(c, "is_bothtuple");
  }

  LLVMBasicBlockRef unreachable_block = codegen_block(c, "unreachable");
  LLVMBasicBlockRef post_block = codegen_block(c, "is_post");

  LLVMValueRef eq_addr = LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value,
    "");
  LLVMBuildCondBr(c->builder, eq_addr, post_block, box_block);

  LLVMPositionBuilderAtEnd(c->builder, box_block);
  LLVMValueRef l_desc = NULL;
  LLVMValueRef l_typeid = NULL;
  bool has_unboxed_sub = (sub_kind & SUBTYPE_KIND_UNBOXED) != 0;
  size_t mask_value = has_unboxed_sub ? 3 : 2;
  LLVMValueRef boxed_mask = LLVMConstInt(c->i32, mask_value, false);

  if(sub_kind == SUBTYPE_KIND_NUMERIC)
  {
    LLVMBuildBr(c->builder, num_block);
  } else if(sub_kind == SUBTYPE_KIND_TUPLE) {
    LLVMBuildBr(c->builder, tuple_block);
  } else {
    l_desc = gendesc_fetch(c, l_value);
    l_typeid = gendesc_typeid(c, l_desc);
    LLVMValueRef left_boxed = LLVMBuildAnd(c->builder, l_typeid, boxed_mask,
      "");
    LLVMValueRef box_switch = LLVMBuildSwitch(c->builder, left_boxed,
      has_unboxed_sub ? post_block : unreachable_block, 0);

    if(num_block != NULL)
      LLVMAddCase(box_switch, LLVMConstInt(c->i32, 0, false), num_block);

    if(tuple_block != NULL)
      LLVMAddCase(box_switch, LLVMConstInt(c->i32, 2, false), tuple_block);
  }

  LLVMValueRef args[3];
  LLVMValueRef is_num = NULL;
  if(num_block != NULL)
  {
    LLVMPositionBuilderAtEnd(c->builder, num_block);

    if(l_desc == NULL)
      l_desc = gendesc_fetch(c, l_value);

    LLVMValueRef r_desc = gendesc_fetch(c, r_value);
    LLVMValueRef same_type = LLVMBuildICmp(c->builder, LLVMIntEQ, l_desc,
      r_desc, "");
    LLVMBuildCondBr(c->builder, same_type, bothnum_block, post_block);

    // Get the machine word size and memcmp without unboxing.
    LLVMPositionBuilderAtEnd(c->builder, bothnum_block);

    if(l_typeid == NULL)
      l_typeid = gendesc_typeid(c, l_desc);

    LLVMValueRef num_sizes = LLVMBuildBitCast(c->builder, c->numeric_sizes,
      c->void_ptr, "");
    args[0] = LLVMBuildZExt(c->builder, l_typeid, c->intptr, "");
    LLVMValueRef size = LLVMBuildInBoundsGEP(c->builder, num_sizes, args, 1,
      "");
    size = LLVMBuildBitCast(c->builder, size, LLVMPointerType(c->i32, 0), "");
    size = LLVMBuildLoad(c->builder, size, "");
    LLVMSetAlignment(size, 4);
    LLVMValueRef one = LLVMConstInt(c->i32, 1, false);
    args[0] = LLVMBuildInBoundsGEP(c->builder, l_value, &one, 1, "");
    args[0] = LLVMBuildBitCast(c->builder, args[0], c->void_ptr, "");
    args[1] = LLVMBuildInBoundsGEP(c->builder, r_value, &one, 1, "");
    args[1] = LLVMBuildBitCast(c->builder, args[1], c->void_ptr, "");
    args[2] = LLVMBuildZExt(c->builder, size, c->intptr, "");
    is_num = gencall_runtime(c, "memcmp", args, 3, "");
    is_num = LLVMBuildICmp(c->builder, LLVMIntEQ, is_num,
      LLVMConstInt(c->i32, 0, false), "");
    LLVMBuildBr(c->builder, post_block);
  }

  LLVMValueRef is_tuple = NULL;
  if(tuple_block != NULL)
  {
    LLVMPositionBuilderAtEnd(c->builder, tuple_block);

    if(l_desc == NULL)
      l_desc = gendesc_fetch(c, l_value);

    LLVMValueRef l_card = gendesc_fieldcount(c, l_desc);
    LLVMValueRef r_desc = gendesc_fetch(c, r_value);
    LLVMValueRef r_card = gendesc_fieldcount(c, r_desc);
    LLVMValueRef card_test = LLVMBuildICmp(c->builder, LLVMIntEQ, l_card,
      r_card, "");
    LLVMBuildCondBr(c->builder, card_test, bothtuple_block, post_block);

    // Call the type-specific __is function, which will unbox the LHS.
    LLVMPositionBuilderAtEnd(c->builder, bothtuple_block);
    reach_method_t* is_fn = reach_method(left_type, TK_BOX, stringtab("__is"),
      NULL);
    pony_assert(is_fn != NULL);

    LLVMValueRef func = gendesc_vtable(c, l_desc, is_fn->vtable_index);

    LLVMTypeRef params[3];
    params[0] = c->object_ptr;
    params[1] = c->object_ptr;
    params[2] = c->descriptor_ptr;
    LLVMTypeRef type = LLVMFunctionType(c->i1, params, 3, false);
    func = LLVMBuildBitCast(c->builder, func, LLVMPointerType(type, 0), "");
    args[0] = l_value;
    args[1] = r_value;
    args[2] = r_desc;
    is_tuple = codegen_call(c, func, args, 3, true);
    LLVMBuildBr(c->builder, post_block);
  }

  LLVMPositionBuilderAtEnd(c->builder, unreachable_block);
  LLVMBuildUnreachable(c->builder);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");
  LLVMValueRef one = LLVMConstInt(c->i1, 1, false);
  LLVMValueRef zero = LLVMConstInt(c->i1, 0, false);
  LLVMAddIncoming(phi, &one, &this_block, 1);

  if(bothnum_block != NULL)
    LLVMAddIncoming(phi, &is_num, &bothnum_block, 1);

  if(bothtuple_block != NULL)
    LLVMAddIncoming(phi, &is_tuple, &bothtuple_block, 1);

  if(num_block != NULL)
    LLVMAddIncoming(phi, &zero, &num_block, 1);

  if(tuple_block != NULL)
    LLVMAddIncoming(phi, &zero, &tuple_block, 1);

  if(has_unboxed_sub)
    LLVMAddIncoming(phi, &zero, &box_block, 1);

  return phi;
}

static LLVMValueRef gen_is_value(compile_t* c, ast_t* left_type,
  ast_t* right_type, LLVMValueRef l_value, LLVMValueRef r_value)
{
  pony_assert(left_type != NULL);

  LLVMTypeRef l_type = LLVMTypeOf(l_value);
  LLVMTypeRef r_type = LLVMTypeOf(r_value);

  bool right_null = right_type == NULL;

  switch(LLVMGetTypeKind(l_type))
  {
    case LLVMIntegerTypeKind:
    {
      // If it's the same type, compare.
      if(l_type == r_type)
        return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");

      // If left_type is a subtype of right_type, check if r_value is a boxed
      // numeric primitive.
      if(right_null || is_subtype(left_type, right_type, NULL, c->opt))
        return raw_is_box(c, left_type, l_value, r_value);

      // It can't have the same identity.
      return LLVMConstInt(c->i1, 0, false);
    }

    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
    {
      // If it's the same type, just compare.
      if(l_type == r_type)
        return LLVMBuildFCmp(c->builder, LLVMRealOEQ, l_value, r_value, "");

      // If left_type is a subtype of right_type, check if r_value is a boxed
      // numeric primitive.
      if(right_null || is_subtype(left_type, right_type, NULL, c->opt))
        return raw_is_box(c, left_type, l_value, r_value);

      // It can't have the same identity.
      return LLVMConstInt(c->i1, 0, false);
    }

    case LLVMStructTypeKind:
    {
      if(LLVMGetTypeKind(r_type) == LLVMStructTypeKind)
      {
        // Pairwise comparison.
        if(ast_childcount(left_type) == ast_childcount(right_type))
          return tuple_is(c, left_type, right_type, l_value, r_value);
      } else if(right_null || !is_known(right_type)) {
        // If right_type is an abstract type, check if r_value is a boxed tuple.
        reach_type_t* r_right = reach_type(c->reach, right_type);
        return tuple_is_box(c, left_type, r_right, l_value, r_value, NULL,
          true, true);
      }

      // It can't have the same identity.
      return LLVMConstInt(c->i1, 0, false);
    }

    case LLVMPointerTypeKind:
    {
      if(LLVMGetTypeKind(r_type) != LLVMPointerTypeKind)
        return gen_is_value(c, right_type, left_type, r_value, l_value);

      l_value = LLVMBuildBitCast(c->builder, l_value, c->object_ptr, "");
      r_value = LLVMBuildBitCast(c->builder, r_value, c->object_ptr, "");

      bool left_known = is_known(left_type);
      bool right_known = !right_null && is_known(right_type);
      reach_type_t* r_left = reach_type(c->reach, left_type);
      reach_type_t* r_right = !right_null ?
        reach_type(c->reach, right_type) : NULL;

      if(!left_known && !right_known)
      {
        int sub_kind = subtype_kind_overlap(r_left, r_right);

        if((sub_kind & SUBTYPE_KIND_BOXED) != 0)
          return box_is_box(c, r_left, l_value, r_value, sub_kind);

        // If the types can be the same, check the address.
        if(sub_kind != SUBTYPE_KIND_NONE)
          return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");
      } else if(left_known && right_known) {
        // If the types are the same, check the address.
        if(r_left == r_right)
          return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");
      } else {
        ast_t* known;
        ast_t* unknown;

        if(left_known)
        {
          known = left_type;
          unknown = right_type;
        } else {
          known = right_type;
          unknown = left_type;
        }

        // If the types can be the same, check the address.
        if(right_null || is_subtype(known, unknown, NULL, c->opt))
          return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");
      }

      // It can't have the same identity.
      return LLVMConstInt(c->i1, 0, false);
    }

    default: {}
  }

  pony_assert(0);
  return NULL;
}

LLVMValueRef gen_is(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  deferred_reification_t* reify = c->frame->reify;
  ast_t* left_type = deferred_reify(reify, ast_type(left), c->opt);
  ast_t* right_type = deferred_reify(reify, ast_type(right), c->opt);

  codegen_debugloc(c, ast);
  LLVMValueRef result = gen_is_value(c, left_type, right_type,
    l_value, r_value);
  codegen_debugloc(c, NULL);

  ast_free_unattached(left_type);
  ast_free_unattached(right_type);
  return result;
}

LLVMValueRef gen_isnt(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  deferred_reification_t* reify = c->frame->reify;
  ast_t* left_type = deferred_reify(reify, ast_type(left), c->opt);
  ast_t* right_type = deferred_reify(reify, ast_type(right), c->opt);

  codegen_debugloc(c, ast);
  LLVMValueRef result = gen_is_value(c, left_type, right_type,
    l_value, r_value);
  result = LLVMBuildNot(c->builder, result, "");
  codegen_debugloc(c, NULL);

  ast_free_unattached(left_type);
  ast_free_unattached(right_type);
  return result;
}

void gen_is_tuple_fun(compile_t* c, reach_type_t* t)
{
  pony_assert(t->underlying == TK_TUPLETYPE);

  reach_method_t* m = reach_method(t, TK_BOX, stringtab("__is"), NULL);

  if(m == NULL)
    return;

  compile_method_t* c_m = (compile_method_t*)m->c_method;

  LLVMTypeRef params[3];
  params[0] = c->object_ptr;
  params[1] = c->object_ptr;
  params[2] = c->descriptor_ptr;
  c_m->func_type = LLVMFunctionType(c->i1, params, 3, false);
  c_m->func = codegen_addfun(c, m->full_name, c_m->func_type, true);

  codegen_startfun(c, c_m->func, NULL, NULL, NULL, false);
  LLVMValueRef l_value = LLVMGetParam(codegen_fun(c), 0);
  LLVMValueRef r_value = LLVMGetParam(codegen_fun(c), 1);
  LLVMValueRef r_desc = LLVMGetParam(codegen_fun(c), 2);

  l_value = gen_unbox(c, t->ast_cap, l_value);

  // We've already checked the cardinality in the checks generated by
  // box_is_box(). Don't recheck it in tuple_is_box().
  LLVMValueRef same_identity = tuple_is_box(c, t->ast_cap, NULL, l_value,
    r_value, r_desc, true, false);
  LLVMBuildRet(c->builder, same_identity);

  codegen_finishfun(c);
}

LLVMValueRef gen_numeric_size_table(compile_t* c)
{
  uint32_t len = c->reach->numeric_type_count;
  if(len == 0)
    return NULL;

  size_t size = len * sizeof(LLVMValueRef);
  LLVMValueRef* args = (LLVMValueRef*)ponyint_pool_alloc_size(size);

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
      size_t type_size = (size_t)LLVMABISizeOfType(c->target_data,
        ((compile_type_t*)t->c_type)->mem_type);
      args[type_id >> 2] = LLVMConstInt(c->i32, type_size, false);
      count++;
    }
  }

  LLVMTypeRef type = LLVMArrayType(c->i32, len);
  LLVMValueRef table = LLVMAddGlobal(c->module, type, "__NumSizeTable");
  LLVMValueRef value = LLVMConstArray(c->i32, args, len);
  LLVMSetInitializer(table, value);
  LLVMSetGlobalConstant(table, true);
  LLVMSetAlignment(table, 4);
  LLVMSetLinkage(table, LLVMPrivateLinkage);

  ponyint_pool_free_size(size, args);

  return table;
}

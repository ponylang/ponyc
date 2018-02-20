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

static LLVMValueRef tuple_is_box(compile_t* c, ast_t* left_type,
  reach_type_t* right_type, LLVMValueRef l_value, LLVMValueRef r_value)
{
  pony_assert(LLVMGetTypeKind(LLVMTypeOf(r_value)) == LLVMPointerTypeKind);

  LLVMValueRef r_desc = gendesc_fetch(c, r_value);
  LLVMValueRef r_typeid = gendesc_typeid(c, r_desc);

  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef post_block = codegen_block(c, "is_tuple_post");
  LLVMValueRef type_switch = LLVMBuildSwitch(c->builder, r_typeid,
    post_block, 0);
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");

  size_t cardinality = ast_childcount(left_type);
  reach_type_t* right;
  size_t i = HASHMAP_BEGIN;

  while((right = reach_type_cache_next(&right_type->subtypes, &i)) != NULL)
  {
    if((ast_id(right->ast_cap) != TK_TUPLETYPE) ||
      (ast_childcount(right->ast_cap) != cardinality))
      continue;

    LLVMBasicBlockRef reltype_block = codegen_block(c, "is_reltype");
    LLVMAddCase(type_switch, LLVMConstInt(c->i32, right->type_id, false),
      reltype_block);
    LLVMPositionBuilderAtEnd(c->builder, reltype_block);

    LLVMValueRef r_unbox = gen_unbox(c, right->ast_cap, r_value);
    LLVMValueRef is = tuple_is(c, left_type, right->ast_cap, l_value, r_unbox);
    LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
    LLVMAddIncoming(phi, &is, &this_block, 1);
    LLVMBuildBr(c->builder, post_block);
  }

  LLVMMoveBasicBlockAfter(post_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef zero = LLVMConstInt(c->i1, 0, false);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
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
    if(sub_kind != SUBTYPE_KIND_TUPLE)
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
    LLVMBuildBr(c->builder, bothtuple_block);
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

  LLVMValueRef r_typeid = NULL;
  if(tuple_block != NULL)
  {
    LLVMPositionBuilderAtEnd(c->builder, tuple_block);
    LLVMValueRef r_desc = gendesc_fetch(c, r_value);
    r_typeid = gendesc_typeid(c, r_desc);
    LLVMValueRef right_tuple = LLVMBuildAnd(c->builder, r_typeid, boxed_mask,
      "");
    right_tuple = LLVMBuildICmp(c->builder, LLVMIntEQ, right_tuple,
      LLVMConstInt(c->i32, 2, false), "");
    LLVMBuildCondBr(c->builder, right_tuple, bothtuple_block, post_block);
  }

  LLVMValueRef is_tuple = NULL;
  if(bothtuple_block != NULL)
  {
    // Call the type-specific __is function, which will unbox the tuples.
    LLVMPositionBuilderAtEnd(c->builder, bothtuple_block);
    reach_method_t* is_fn = reach_method(left_type, TK_BOX, stringtab("__is"),
      NULL);
    pony_assert(is_fn != NULL);

    if(l_desc == NULL)
      l_desc = gendesc_fetch(c, l_value);

    LLVMValueRef func = gendesc_vtable(c, l_desc, is_fn->vtable_index);

    if(r_typeid == NULL)
    {
      LLVMValueRef r_desc = gendesc_fetch(c, r_value);
      r_typeid = gendesc_typeid(c, r_desc);
    }

    LLVMTypeRef params[3];
    params[0] = c->object_ptr;
    params[1] = c->object_ptr;
    params[2] = c->i32;
    LLVMTypeRef type = LLVMFunctionType(c->i1, params, 3, false);
    func = LLVMBuildBitCast(c->builder, func, LLVMPointerType(type, 0), "");
    args[0] = l_value;
    args[1] = r_value;
    args[2] = r_typeid;
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
  LLVMTypeRef l_type = LLVMTypeOf(l_value);
  LLVMTypeRef r_type = LLVMTypeOf(r_value);

  switch(LLVMGetTypeKind(l_type))
  {
    case LLVMIntegerTypeKind:
    {
      // If it's the same type, compare.
      if(l_type == r_type)
        return LLVMBuildICmp(c->builder, LLVMIntEQ, l_value, r_value, "");

      // If left_type is a subtype of right_type, check if r_value is a boxed
      // numeric primitive.
      if(is_subtype(left_type, right_type, NULL, c->opt))
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
      if(is_subtype(left_type, right_type, NULL, c->opt))
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
      } else if(!is_known(right_type)) {
        // If right_type is an abstract type, check if r_value is a boxed tuple.
        reach_type_t* r_right = reach_type(c->reach, right_type);
        return tuple_is_box(c, left_type, r_right, l_value, r_value);
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
      bool right_known = is_known(right_type);
      reach_type_t* r_left = reach_type(c->reach, left_type);
      reach_type_t* r_right = reach_type(c->reach, right_type);

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
        if(is_subtype(known, unknown, NULL, c->opt))
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
  params[2] = c->i32;
  c_m->func_type = LLVMFunctionType(c->i1, params, 3, false);
  c_m->func = codegen_addfun(c, m->full_name, c_m->func_type, true);

  codegen_startfun(c, c_m->func, NULL, NULL, NULL, false);
  LLVMValueRef l_value = LLVMGetParam(codegen_fun(c), 0);
  LLVMValueRef r_value = LLVMGetParam(codegen_fun(c), 1);
  LLVMValueRef r_id = LLVMGetParam(codegen_fun(c), 2);

  l_value = gen_unbox(c, t->ast_cap, l_value);
  LLVMBasicBlockRef notreltype_block = codegen_block(c, "not_reltype");
  LLVMValueRef type_switch = LLVMBuildSwitch(c->builder, r_id, notreltype_block,
    0);

  pony_assert(m->tuple_is_types != NULL);
  reach_type_t* right;
  size_t i = HASHMAP_BEGIN;

  while((right = reach_type_cache_next(m->tuple_is_types, &i)) != NULL)
  {
    LLVMBasicBlockRef reltype_block = codegen_block(c, "reltype");
    LLVMAddCase(type_switch, LLVMConstInt(c->i32, right->type_id, false),
      reltype_block);
    LLVMPositionBuilderAtEnd(c->builder, reltype_block);

    LLVMValueRef r_unbox = gen_unbox(c, right->ast_cap, r_value);
    LLVMBuildRet(c->builder, tuple_is(c, t->ast_cap, right->ast_cap, l_value,
      r_unbox));
  }

  LLVMMoveBasicBlockAfter(notreltype_block, LLVMGetInsertBlock(c->builder));
  LLVMPositionBuilderAtEnd(c->builder, notreltype_block);
  LLVMBuildRet(c->builder, LLVMConstInt(c->i1, 0, false));

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

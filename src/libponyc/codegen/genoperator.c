#include "genoperator.h"
#include "genbox.h"
#include "gencall.h"
#include "gendesc.h"
#include "genexpr.h"
#include "genmutate.h"
#include "gentagged.h"
#include "genreference.h"
#include "genname.h"
#include "gentype.h"
#include "../pkg/platformfuns.h"
#include "../reach/reach.h"
#include "../type/subtype.h"
#include "../type/typealias.h"
#include "ponyassert.h"

static LLVMValueRef assign_rvalue(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value);

static bool is_always_true(LLVMValueRef val)
{
  if(!LLVMIsAConstantInt(val))
    return false;

  return LLVMConstIntGetZExtValue(val) == 1;
}

static bool is_always_false(LLVMValueRef val)
{
  if(!LLVMIsAConstantInt(val))
    return false;

  return LLVMConstIntGetZExtValue(val) == 0;
}

bool is_fp(LLVMValueRef val)
{
  LLVMTypeRef type = LLVMTypeOf(val);

  switch(LLVMGetTypeKind(type))
  {
    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
      return true;

    default: {}
  }

  return false;
}

static LLVMValueRef make_binop(compile_t* c, ast_t* left, ast_t* right,
  build_binop build_f, build_binop build_i)
{
  ast_t* mutate_ast = c->frame->mutate_ast;
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMValueRef result;

  if(is_fp(l_value))
    result = build_f(c->builder, l_value, r_value, "");
  else
    result = build_i(c->builder, l_value, r_value, "");

  if(c->opt->mutate && (mutate_ast != NULL))
  {
    result = mutate_cr_binop(c, mutate_ast, left, right,
      l_value, r_value, result, build_f, build_i);

    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow;
      if(is_fp(l_value))
        shadow = build_f(c->builder, l_shadow, r_shadow, "");
      else
        shadow = build_i(c->builder, l_shadow, r_shadow, "");
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    result = mutate_uoi_insert(c, mutate_ast, result, l_value, r_value);
  }
  else if(c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow;
      if(is_fp(l_value))
        shadow = build_f(c->builder, l_shadow, r_shadow, "");
      else
        shadow = build_i(c->builder, l_shadow, r_shadow, "");
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }
  }

  return result;
}

LLVMValueRef make_divrem_inner(compile_t* c,
  LLVMValueRef l_value, LLVMValueRef r_value, bool sign,
  build_binop build_f, build_binop build_ui, build_binop build_si,
  bool safe, bool is_div)
{
  LLVMTypeRef r_type = LLVMTypeOf(r_value);

  if(!is_fp(r_value) && LLVMIsConstant(r_value))
  {
    LLVMValueRef neg_one = LLVMConstAllOnes(r_type);
    LLVMValueRef is_neg_one = LLVMBuildICmp(c->builder, LLVMIntEQ,
      r_value, neg_one, "");

    if(is_always_true(is_neg_one) && LLVMIsConstant(l_value) && sign)
    {
      uint64_t width = LLVMGetIntTypeWidth(r_type);
      LLVMValueRef v_min = LLVMConstInt(r_type, 0, false);
      v_min = LLVMConstNot(v_min);
      v_min = LLVMBuildShl(c->builder, v_min,
        LLVMConstInt(r_type, width - 1, false), "");
      LLVMValueRef cmp = LLVMBuildICmp(c->builder, LLVMIntEQ,
        l_value, v_min, "");
      if(is_always_true(cmp))
        return is_div ? v_min : LLVMConstInt(r_type, 0, false);
    }
  }

  if(is_fp(l_value))
    return build_f(c->builder, l_value, r_value, "");

  LLVMBasicBlockRef insert;
  LLVMBasicBlockRef nonzero_block;
  LLVMBasicBlockRef no_overflow_block;
  LLVMBasicBlockRef post_block = NULL;
  LLVMValueRef zero;
  LLVMValueRef overflow_val = NULL;

  if(safe)
  {
    insert = LLVMGetInsertBlock(c->builder);
    nonzero_block = codegen_block(c, "div_nonzero");
    if(sign)
      no_overflow_block = codegen_block(c, "div_no_overflow");
    post_block = codegen_block(c, "div_post");

    zero = LLVMConstInt(r_type, 0, false);
    LLVMValueRef cmp = LLVMBuildICmp(c->builder, LLVMIntNE, r_value, zero, "");
    LLVMBuildCondBr(c->builder, cmp, nonzero_block, post_block);

    LLVMPositionBuilderAtEnd(c->builder, nonzero_block);

    if(sign)
    {
      uint64_t width = LLVMGetIntTypeWidth(r_type);
      LLVMValueRef neg_one = LLVMConstInt(r_type, 0, false);
      neg_one = LLVMConstNot(neg_one);
      LLVMValueRef denom_good = LLVMBuildICmp(c->builder, LLVMIntNE, r_value,
        neg_one, "");
      LLVMValueRef v_min = LLVMBuildShl(c->builder, neg_one,
        LLVMConstInt(r_type, width - 1, false), "");
      LLVMValueRef numer_good = LLVMBuildICmp(c->builder, LLVMIntNE, l_value,
        v_min, "");
      LLVMValueRef no_overflow = LLVMBuildOr(c->builder, numer_good, denom_good,
        "");
      LLVMBuildCondBr(c->builder, no_overflow, no_overflow_block, post_block);

      overflow_val = is_div ? v_min : zero;

      LLVMPositionBuilderAtEnd(c->builder, no_overflow_block);
    }
  }

  LLVMValueRef result;

  if(sign)
    result = build_si(c->builder, l_value, r_value, "");
  else
    result = build_ui(c->builder, l_value, r_value, "");

  if(safe)
  {
    LLVMBuildBr(c->builder, post_block);

    LLVMPositionBuilderAtEnd(c->builder, post_block);
    LLVMValueRef phi = LLVMBuildPhi(c->builder, r_type, "");
    LLVMAddIncoming(phi, &zero, &insert, 1);
    if(sign)
    {
      LLVMAddIncoming(phi, &overflow_val, &nonzero_block, 1);
      LLVMAddIncoming(phi, &result, &no_overflow_block, 1);
    } else {
      LLVMAddIncoming(phi, &result, &nonzero_block, 1);
    }

    return phi;
  }

  return result;
}

static LLVMValueRef make_divrem(compile_t* c, ast_t* left, ast_t* right,
  build_binop build_f, build_binop build_ui, build_binop build_si,
  bool safe, bool is_div)
{
  ast_t* mutate_ast = c->frame->mutate_ast;

  ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
  bool sign = is_signed(type);
  ast_free_unattached(type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMValueRef result = make_divrem_inner(c, l_value, r_value, sign,
    build_f, build_ui, build_si, safe, is_div);

  if(c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);

    if(mutate_ast != NULL)
    {
      result = mutate_cr_divrem(c, mutate_ast, left, right,
        l_value, r_value, result, sign, safe, is_div,
        build_f, build_ui, build_si);
    }

    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow;
      if(is_fp(l_value))
        shadow = build_f(c->builder, l_shadow, r_shadow, "");
      else
        shadow = mutate_safe_divrem_shadow(c->builder, l_shadow, r_shadow,
          sign, is_div);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    if(mutate_ast != NULL)
      result = mutate_uoi_insert(c, mutate_ast, result, l_value, r_value);
  }

  return result;
}

LLVMValueRef make_cmp_value(compile_t* c, bool sign,
  LLVMValueRef l_value, LLVMValueRef r_value, LLVMRealPredicate cmp_f,
  LLVMIntPredicate cmp_si, LLVMIntPredicate cmp_ui, bool safe)
{
  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
  {
    if(is_fp(l_value))
      return LLVMBuildFCmp(c->builder, cmp_f, l_value, r_value, "");

    if(sign)
      return LLVMBuildICmp(c->builder, cmp_si, l_value, r_value, "");

    return LLVMBuildICmp(c->builder, cmp_ui, l_value, r_value, "");
  }

  if(is_fp(l_value))
  {
    LLVMValueRef result = LLVMBuildFCmp(c->builder, cmp_f, l_value, r_value,
      "");
    if(!safe)
      LLVMSetUnsafeAlgebra(result);
    return result;
  }

  if(sign)
    return LLVMBuildICmp(c->builder, cmp_si, l_value, r_value, "");

  return LLVMBuildICmp(c->builder, cmp_ui, l_value, r_value, "");
}

static LLVMValueRef make_cmp(compile_t* c, ast_t* left, ast_t* right,
  LLVMRealPredicate cmp_f, LLVMIntPredicate cmp_si, LLVMIntPredicate cmp_ui,
  bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;
  ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
  bool sign = is_signed(type);
  ast_free_unattached(type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  LLVMValueRef result =
    make_cmp_value(c, sign, l_value, r_value, cmp_f, cmp_si, cmp_ui, safe);

  if((result != NULL) && c->opt->mutate && (mutate_ast != NULL))
  {
    result = mutate_cr_cmp(c, mutate_ast, left, right,
      l_value, r_value, result, sign, cmp_f, cmp_si, cmp_ui, safe);
  }

  if((result != NULL) && c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow =
        make_cmp_value(c, sign, l_shadow, r_shadow,
          cmp_f, cmp_si, cmp_ui, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }
  }

  return result;
}

static LLVMValueRef make_short_circuit(compile_t* c, ast_t* left, ast_t* right,
  bool is_and)
{
  LLVMBasicBlockRef left_block = codegen_block(c, "sc_left");
  LLVMBuildBr(c->builder, left_block);

  LLVMPositionBuilderAtEnd(c->builder, left_block);
  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  if(LLVMIsAConstantInt(l_value))
  {
    if(is_and)
    {
      if(is_always_false(l_value))
        return l_value;
    } else {
      if(is_always_true(l_value))
        return l_value;
    }

    return gen_expr(c, right);
  }

  LLVMBasicBlockRef left_exit_block = LLVMGetInsertBlock(c->builder);

  LLVMBasicBlockRef right_block = codegen_block(c, "sc_right");
  LLVMBasicBlockRef post_block = codegen_block(c, "sc_post");

  if(is_and)
    LLVMBuildCondBr(c->builder, l_value, right_block, post_block);
  else
    LLVMBuildCondBr(c->builder, l_value, post_block, right_block);

  LLVMPositionBuilderAtEnd(c->builder, right_block);
  LLVMValueRef r_value = gen_expr(c, right);

  if(r_value == NULL)
    return NULL;

  LLVMBasicBlockRef right_exit_block = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, post_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");

  LLVMAddIncoming(phi, &l_value, &left_exit_block, 1);
  LLVMAddIncoming(phi, &r_value, &right_exit_block, 1);

  if(LLVMIsAConstantInt(r_value))
  {
    if(is_and)
    {
      if(is_always_false(r_value))
        return r_value;
    } else {
      if(is_always_true(r_value))
        return r_value;
    }

    return l_value;
  }

  return phi;
}

static LLVMValueRef assign_local(compile_t* c, LLVMValueRef l_value,
  LLVMValueRef r_value, ast_t* l_type, ast_t* r_type)
{
  reach_type_t* t = reach_type(c->reach, l_type, c->opt);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  LLVMValueRef result = LLVMBuildLoad2(c->builder,
    LLVMGetAllocatedType(l_value), l_value, "");
  result = gen_assign_cast(c, c_t->use_type, result, l_type);

  // Cast the rvalue appropriately.
  r_value = gen_assign_cast(c, c_t->mem_type, r_value, r_type);

  if(r_value == NULL)
    return NULL;

  // Store to the local.
  LLVMBuildStore(c->builder, r_value, l_value);

  return result;
}

static LLVMValueRef assign_field(compile_t* c, LLVMValueRef l_value,
  LLVMValueRef r_value, ast_t* l_type, ast_t* r_type,
  bool owner_has_finaliser)
{
  reach_type_t* t = reach_type(c->reach, l_type, c->opt);
  pony_assert(t != NULL);
  compile_type_t* c_t = (compile_type_t*)t->c_type;

  LLVMValueRef result = LLVMBuildLoad2(c->builder, c_t->mem_type, l_value, "");

  // Cast the rvalue appropriately.
  r_value = gen_assign_cast(c, c_t->mem_type, r_value, r_type);

  if(r_value == NULL)
    return NULL;

  LLVMValueRef store = LLVMBuildStore(c->builder, r_value, l_value);

  if(owner_has_finaliser)
    LLVMSetVolatile(store, true);

  return gen_assign_cast(c, c_t->use_type, result, l_type);
}

static bool assign_tuple(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value)
{
  // Unfold type aliases to get the actual tuple element types.
  ast_t* iter_type = r_type;
  ast_t* unfolded = NULL;

  if(ast_id(r_type) == TK_TYPEALIASREF)
  {
    unfolded = typealias_unfold(r_type);

    if(unfolded != NULL)
      iter_type = unfolded;
  }

  // Handle assignment for each component.
  ast_t* child = ast_child(left);
  ast_t* rtype_child = ast_child(iter_type);
  int i = 0;

  while(child != NULL)
  {
    ast_t* expr = NULL;

    switch(ast_id(child))
    {
      case TK_SEQ:
        // The actual tuple expression is inside a sequence node.
        expr = ast_child(child);
        break;

      case TK_LET:
      case TK_VAR:
      case TK_TUPLE:
        expr = child;
        break;

      case TK_DONTCAREREF:
        // Ignore this element.
        break;

      default:
        pony_assert(0);
    }

    if(expr != NULL)
    {
      // Extract the tuple value.
      LLVMValueRef rvalue_child = LLVMBuildExtractValue(c->builder, r_value, i,
        "");

      // Recurse, assigning pairwise.
      if(assign_rvalue(c, expr, rtype_child, rvalue_child) == NULL)
        return false;
    }

    i++;
    child = ast_sibling(child);
    rtype_child = ast_sibling(rtype_child);
  }

  pony_assert(rtype_child == NULL);

  if(unfolded != NULL)
    ast_free_unattached(unfolded);

  return true;
}

static LLVMValueRef assign_rvalue(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value)
{
  deferred_reification_t* reify = c->frame->reify;

  switch(ast_id(left))
  {
    case TK_SEQ:
      // The actual expression is inside a sequence node.
      while(ast_id(left) == TK_SEQ)
      {
        pony_assert(ast_childcount(left) == 1);
        left = ast_child(left);
      }
      return assign_rvalue(c, left, r_type, r_value);

    case TK_VAR:
    case TK_LET:
    {
      // Generate the locals.
      if(gen_localdecl(c, left) == NULL)
        return NULL;

      return assign_rvalue(c, ast_child(left), r_type, r_value);
    }

    case TK_MATCH_CAPTURE:
      // The alloca has already been generated.
      return assign_rvalue(c, ast_child(left), r_type, r_value);

    case TK_FVARREF:
    case TK_FLETREF:
    {
      // The result is the previous value of the field.
      LLVMValueRef l_value = gen_fieldptr(c, left);
      ast_t* p_type = deferred_reify(reify, ast_type(ast_child(left)), c->opt);
      ast_t* l_type = deferred_reify(reify, ast_type(left), c->opt);

      reach_type_t* p_t = reach_type(c->reach, p_type, c->opt);
      bool has_finaliser = (p_t != NULL) &&
        (((compile_type_t*)p_t->c_type)->final_fn != NULL);

      LLVMValueRef ret = assign_field(c, l_value, r_value, l_type, r_type,
        has_finaliser);

      ast_free_unattached(p_type);
      ast_free_unattached(l_type);
      return ret;
    }

    case TK_TUPLEELEMREF:
    {
      // The result is the previous value of the tuple element.
      LLVMValueRef l_value = gen_tupleelemptr(c, left);
      ast_t* p_type = deferred_reify(reify, ast_type(ast_child(left)), c->opt);
      ast_t* l_type = deferred_reify(reify, ast_type(left), c->opt);

      LLVMValueRef r = assign_field(c, l_value, r_value, l_type, r_type,
        false);

      ast_free_unattached(p_type);
      ast_free_unattached(l_type);
      return r;
    }

    case TK_EMBEDREF:
    {
      // Do nothing. The embed field was already passed as the receiver.
      // But we can't return GEN_NOVALUE - that's reserved only for branches
      // that "jump away". So we just return the r_value we were given.
      // The type system ensures that nobody will be able to use this value.
      return r_value;
    }

    case TK_VARREF:
    {
      // The result is the previous value of the local.
      const char* name = ast_name(ast_child(left));
      codegen_local_lifetime_start(c, name);
      LLVMValueRef l_value = codegen_getlocal(c, name);
      ast_t* l_type = deferred_reify(reify, ast_type(left), c->opt);

      LLVMValueRef ret = assign_local(c, l_value, r_value, l_type, r_type);

      ast_free_unattached(l_type);
      return ret;
    }

    case TK_DONTCARE:
    case TK_DONTCAREREF:
    case TK_MATCH_DONTCARE:
    {
      // Do nothing.
      // But we can't return GEN_NOVALUE - that's reserved only for branches
      // that "jump away". So we just return the r_value we were given.
      // The type system ensures that nobody will be able to use this value.
      return r_value;
    }

    case TK_TUPLE:
    {
      // If the l_value is a tuple, assemble it as the result.
      LLVMValueRef result = gen_expr(c, left);

      if(result == NULL)
        return NULL;

      if(ast_id(r_type) == TK_UNIONTYPE)
      {
        reach_type_t* t = reach_type(c->reach, r_type, c->opt);
        pony_assert(t != NULL);

        LLVMValueRef r_desc = gentagged_fetch_desc_or_heap(c, r_value, t,
          "op");

        LLVMValueRef r_typeid = gendesc_typeid(c, r_desc);

        LLVMBasicBlockRef unreachable_block =
          codegen_block(c, "destructure_unreachable");
        LLVMBasicBlockRef post_block =
          codegen_block(c, "destructure_post");
        LLVMValueRef type_switch = LLVMBuildSwitch(c->builder, r_typeid,
          unreachable_block, 0);

        reach_type_t* sub;
        size_t si = HASHMAP_BEGIN;

        while((sub = reach_type_cache_next(&t->subtypes, &si)) != NULL)
        {
          if(sub->underlying != TK_TUPLETYPE)
            continue;

          LLVMBasicBlockRef sub_block =
            codegen_block(c, "destructure_variant");
          LLVMAddCase(type_switch,
            LLVMConstInt(c->i32, sub->type_id, false), sub_block);
          LLVMPositionBuilderAtEnd(c->builder, sub_block);

          LLVMValueRef r_unbox = gen_unbox(c, sub->ast_cap, r_value);

          if(!assign_tuple(c, left, sub->ast_cap, r_unbox))
            return NULL;

          LLVMBuildBr(c->builder, post_block);
        }

        LLVMMoveBasicBlockAfter(unreachable_block,
          LLVMGetInsertBlock(c->builder));
        LLVMPositionBuilderAtEnd(c->builder, unreachable_block);
        LLVMBuildUnreachable(c->builder);

        LLVMMoveBasicBlockAfter(post_block, unreachable_block);
        LLVMPositionBuilderAtEnd(c->builder, post_block);
      }
      else
      {
        if(!assign_tuple(c, left, r_type, r_value))
          return NULL;
      }

      // Return the original tuple.
      return result;
    }

    case TK_ID:
    {
      // We have recursed here from a VAR, LET or MATCH_CAPTURE.
      const char* name = ast_name(left);
      codegen_local_lifetime_start(c, name);
      LLVMValueRef l_value = codegen_getlocal(c, name);
      ast_t* l_type = deferred_reify(reify, ast_type(ast_parent(left)), c->opt);

      LLVMValueRef ret = assign_local(c, l_value, r_value, l_type, r_type);

      ast_free_unattached(l_type);
      return ret;
    }

    default: {}
  }

  pony_assert(0);
  return NULL;
}

static LLVMValueRef make_unsafe_fadd(LLVMBuilderRef builder, LLVMValueRef left,
  LLVMValueRef right, const char *name)
{
  LLVMValueRef result = LLVMBuildFAdd(builder, left, right, name);
  if(LLVMIsAInstruction(result))
    LLVMSetUnsafeAlgebra(result);
  return result;
}

static LLVMValueRef make_unsafe_fsub(LLVMBuilderRef builder, LLVMValueRef left,
  LLVMValueRef right, const char *name)
{
  LLVMValueRef result = LLVMBuildFSub(builder, left, right, name);
  if(LLVMIsAInstruction(result))
    LLVMSetUnsafeAlgebra(result);
  return result;
}

static LLVMValueRef make_unsafe_fmul(LLVMBuilderRef builder, LLVMValueRef left,
  LLVMValueRef right, const char *name)
{
  LLVMValueRef result = LLVMBuildFMul(builder, left, right, name);
  if(LLVMIsAInstruction(result))
    LLVMSetUnsafeAlgebra(result);
  return result;
}

static LLVMValueRef make_unsafe_fdiv(LLVMBuilderRef builder, LLVMValueRef left,
  LLVMValueRef right, const char *name)
{
  LLVMValueRef result = LLVMBuildFDiv(builder, left, right, name);
  if(LLVMIsAInstruction(result))
    LLVMSetUnsafeAlgebra(result);
  return result;
}

static LLVMValueRef make_unsafe_frem(LLVMBuilderRef builder, LLVMValueRef left,
  LLVMValueRef right, const char *name)
{
  LLVMValueRef result = LLVMBuildFRem(builder, left, right, name);
  if(LLVMIsAInstruction(result))
    LLVMSetUnsafeAlgebra(result);
  return result;
}

static LLVMValueRef make_unsafe_fneg(LLVMBuilderRef builder, LLVMValueRef op,
  const char *name)
{
  LLVMValueRef result = LLVMBuildFNeg(builder, op, name);
  if(LLVMIsAInstruction(result))
    LLVMSetUnsafeAlgebra(result);
  return result;
}

static LLVMValueRef make_nsw_neg(LLVMBuilderRef builder, LLVMValueRef op,
  const char* name)
{
  LLVMValueRef result = LLVMBuildNeg(builder, op, name);
  if(LLVMIsAInstruction(result))
    LLVMSetNoSignedWrap(result);
  return result;
}

static LLVMValueRef make_nuw_neg(LLVMBuilderRef builder, LLVMValueRef op,
  const char* name)
{
  LLVMValueRef result = LLVMBuildNeg(builder, op, name);
  if(LLVMIsAInstruction(result))
    LLVMSetNoUnsignedWrap(result);
  return result;
}

static bool left_is_signed(compile_t* c, ast_t* left)
{
  ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
  bool sign = is_signed(type);
  ast_free_unattached(type);
  return sign;
}

LLVMValueRef gen_add(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  if(c->opt->mutate && (c->frame->mutate_ast != NULL))
  {
    bool do_aor = mutate_site_enabled(c, "aor", c->frame->mutate_ast);
    bool do_amr = mutate_site_enabled(c, "amr", c->frame->mutate_ast);

    if(do_aor || do_amr)
    {
      if(safe)
      {
        bool sign = left_is_signed(c, left);
        mutate_variant_t variants[3];
        size_t count = 0;

        if(do_aor)
        {
          variants[count++] = (mutate_variant_t)
            {LLVMBuildFSub, LLVMBuildSub, "sub", NULL};
          variants[count++] = (mutate_variant_t)
            {LLVMBuildFMul, LLVMBuildMul, "mul", NULL};
        }
        if(do_amr)
        {
          variants[count++] = (mutate_variant_t)
            {make_unsafe_fadd,
             sign ? LLVMBuildNSWAdd : LLVMBuildNUWAdd,
             "add_unsafe", "amr"};
        }

        return mutate_binop(c, c->frame->mutate_ast, left, right,
          LLVMBuildFAdd, LLVMBuildAdd, "add", "aor", variants, count);
      }

      bool sign = left_is_signed(c, left);
      mutate_variant_t variants[3];
      size_t count = 0;

      if(do_aor)
      {
        variants[count++] = (mutate_variant_t)
          {make_unsafe_fsub,
           sign ? LLVMBuildNSWSub : LLVMBuildNUWSub, "sub", NULL};
        variants[count++] = (mutate_variant_t)
          {make_unsafe_fmul,
           sign ? LLVMBuildNSWMul : LLVMBuildNUWMul, "mul", NULL};
      }
      if(do_amr)
      {
        variants[count++] = (mutate_variant_t)
          {LLVMBuildFAdd, LLVMBuildAdd, "add_wrapping", "amr"};
      }

      return mutate_binop(c, c->frame->mutate_ast, left, right,
        make_unsafe_fadd, sign ? LLVMBuildNSWAdd : LLVMBuildNUWAdd,
        "add", "aor", variants, count);
    }
  }

  if(safe)
    return make_binop(c, left, right, LLVMBuildFAdd, LLVMBuildAdd);

  bool sign = left_is_signed(c, left);

  return make_binop(c, left, right, make_unsafe_fadd,
    sign ? LLVMBuildNSWAdd : LLVMBuildNUWAdd);
}

LLVMValueRef gen_sub(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  if(c->opt->mutate && (c->frame->mutate_ast != NULL))
  {
    bool do_aor = mutate_site_enabled(c, "aor", c->frame->mutate_ast);
    bool do_amr = mutate_site_enabled(c, "amr", c->frame->mutate_ast);

    if(do_aor || do_amr)
    {
      if(safe)
      {
        bool sign = left_is_signed(c, left);
        mutate_variant_t variants[3];
        size_t count = 0;

        if(do_aor)
        {
          variants[count++] = (mutate_variant_t)
            {LLVMBuildFAdd, LLVMBuildAdd, "add", NULL};
          variants[count++] = (mutate_variant_t)
            {LLVMBuildFMul, LLVMBuildMul, "mul", NULL};
        }
        if(do_amr)
        {
          variants[count++] = (mutate_variant_t)
            {make_unsafe_fsub,
             sign ? LLVMBuildNSWSub : LLVMBuildNUWSub,
             "sub_unsafe", "amr"};
        }

        return mutate_binop(c, c->frame->mutate_ast, left, right,
          LLVMBuildFSub, LLVMBuildSub, "sub", "aor", variants, count);
      }

      bool sign = left_is_signed(c, left);
      mutate_variant_t variants[3];
      size_t count = 0;

      if(do_aor)
      {
        variants[count++] = (mutate_variant_t)
          {make_unsafe_fadd,
           sign ? LLVMBuildNSWAdd : LLVMBuildNUWAdd, "add", NULL};
        variants[count++] = (mutate_variant_t)
          {make_unsafe_fmul,
           sign ? LLVMBuildNSWMul : LLVMBuildNUWMul, "mul", NULL};
      }
      if(do_amr)
      {
        variants[count++] = (mutate_variant_t)
          {LLVMBuildFSub, LLVMBuildSub, "sub_wrapping", "amr"};
      }

      return mutate_binop(c, c->frame->mutate_ast, left, right,
        make_unsafe_fsub, sign ? LLVMBuildNSWSub : LLVMBuildNUWSub,
        "sub", "aor", variants, count);
    }
  }

  if(safe)
    return make_binop(c, left, right, LLVMBuildFSub, LLVMBuildSub);

  bool sign = left_is_signed(c, left);

  return make_binop(c, left, right, make_unsafe_fsub,
    sign ? LLVMBuildNSWSub : LLVMBuildNUWSub);
}

LLVMValueRef gen_mul(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  if(c->opt->mutate && (c->frame->mutate_ast != NULL))
  {
    bool do_aor = mutate_site_enabled(c, "aor", c->frame->mutate_ast);
    bool do_amr = mutate_site_enabled(c, "amr", c->frame->mutate_ast);

    if(do_aor || do_amr)
    {
      if(safe)
      {
        bool sign = left_is_signed(c, left);
        mutate_variant_t variants[3];
        size_t count = 0;

        if(do_aor)
        {
          variants[count++] = (mutate_variant_t)
            {LLVMBuildFAdd, LLVMBuildAdd, "add", NULL};
          variants[count++] = (mutate_variant_t)
            {LLVMBuildFSub, LLVMBuildSub, "sub", NULL};
        }
        if(do_amr)
        {
          variants[count++] = (mutate_variant_t)
            {make_unsafe_fmul,
             sign ? LLVMBuildNSWMul : LLVMBuildNUWMul,
             "mul_unsafe", "amr"};
        }

        return mutate_binop(c, c->frame->mutate_ast, left, right,
          LLVMBuildFMul, LLVMBuildMul, "mul", "aor", variants, count);
      }

      bool sign = left_is_signed(c, left);
      mutate_variant_t variants[3];
      size_t count = 0;

      if(do_aor)
      {
        variants[count++] = (mutate_variant_t)
          {make_unsafe_fadd,
           sign ? LLVMBuildNSWAdd : LLVMBuildNUWAdd, "add", NULL};
        variants[count++] = (mutate_variant_t)
          {make_unsafe_fsub,
           sign ? LLVMBuildNSWSub : LLVMBuildNUWSub, "sub", NULL};
      }
      if(do_amr)
      {
        variants[count++] = (mutate_variant_t)
          {LLVMBuildFMul, LLVMBuildMul, "mul_wrapping", "amr"};
      }

      return mutate_binop(c, c->frame->mutate_ast, left, right,
        make_unsafe_fmul, sign ? LLVMBuildNSWMul : LLVMBuildNUWMul,
        "mul", "aor", variants, count);
    }
  }

  if(safe)
    return make_binop(c, left, right, LLVMBuildFMul, LLVMBuildMul);

  bool sign = left_is_signed(c, left);

  return make_binop(c, left, right, make_unsafe_fmul,
    sign ? LLVMBuildNSWMul : LLVMBuildNUWMul);
}

LLVMValueRef gen_div(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  if(c->opt->mutate && (c->frame->mutate_ast != NULL))
  {
    bool do_aor = mutate_site_enabled(c, "aor", c->frame->mutate_ast);
    bool do_amr = mutate_site_enabled(c, "amr", c->frame->mutate_ast);

    if(do_aor || do_amr)
    {
      mutate_divrem_variant_t variants[2];
      size_t count = 0;

      if(do_aor)
      {
        variants[count++] = (mutate_divrem_variant_t)
          {safe ? LLVMBuildFRem : make_unsafe_frem,
            LLVMBuildURem, LLVMBuildSRem, false, -1, "rem", NULL};
      }
      if(do_amr)
      {
        variants[count++] = (mutate_divrem_variant_t)
          {safe ? make_unsafe_fdiv : LLVMBuildFDiv,
            LLVMBuildUDiv, LLVMBuildSDiv, true, !safe,
            safe ? "div_unsafe" : "div_wrapping", "amr"};
      }

      return mutate_divrem(c, c->frame->mutate_ast, left, right,
        safe ? LLVMBuildFDiv : make_unsafe_fdiv,
        LLVMBuildUDiv, LLVMBuildSDiv, safe, true,
        "div", "aor", variants, count);
    }
  }

  return make_divrem(c, left, right,
    safe ? LLVMBuildFDiv : make_unsafe_fdiv, LLVMBuildUDiv, LLVMBuildSDiv,
    safe, true);
}

LLVMValueRef gen_rem(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  if(c->opt->mutate && (c->frame->mutate_ast != NULL))
  {
    bool do_aor = mutate_site_enabled(c, "aor", c->frame->mutate_ast);
    bool do_amr = mutate_site_enabled(c, "amr", c->frame->mutate_ast);

    if(do_aor || do_amr)
    {
      mutate_divrem_variant_t variants[2];
      size_t count = 0;

      if(do_aor)
      {
        variants[count++] = (mutate_divrem_variant_t)
          {safe ? LLVMBuildFDiv : make_unsafe_fdiv,
            LLVMBuildUDiv, LLVMBuildSDiv, true, -1, "div", NULL};
      }
      if(do_amr)
      {
        variants[count++] = (mutate_divrem_variant_t)
          {safe ? make_unsafe_frem : LLVMBuildFRem,
            LLVMBuildURem, LLVMBuildSRem, false, !safe,
            safe ? "rem_unsafe" : "rem_wrapping", "amr"};
      }

      return mutate_divrem(c, c->frame->mutate_ast, left, right,
        safe ? LLVMBuildFRem : make_unsafe_frem,
        LLVMBuildURem, LLVMBuildSRem, safe, false,
        "rem", "aor", variants, count);
    }
  }

  return make_divrem(c, left, right,
    safe ? LLVMBuildFRem : make_unsafe_frem, LLVMBuildURem, LLVMBuildSRem,
    safe, false);
}

LLVMValueRef gen_neg(compile_t* c, ast_t* ast, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;
  LLVMValueRef value = gen_expr(c, ast);

  if(value == NULL)
    return NULL;

  LLVMValueRef neg_result;

  if(c->opt->mutate && (mutate_ast != NULL))
  {
    bool do_uoi = mutate_site_enabled(c, "uoi", mutate_ast);
    bool do_amr = mutate_site_enabled(c, "amr", mutate_ast);

    if(do_uoi || do_amr)
    {
      bool fp = is_fp(value);
      bool is_const_fp = LLVMIsAConstantFP(value) != NULL;
      bool is_const_int = LLVMIsAConstantInt(value) != NULL;

      if(is_const_fp)
        neg_result = LLVMBuildFNeg(c->builder, value, "");
      else if(is_const_int)
        neg_result = LLVMConstNeg(value);
      else if(fp)
        neg_result = (safe ? LLVMBuildFNeg : make_unsafe_fneg)(
          c->builder, value, "");
      else if(safe)
        neg_result = LLVMBuildNeg(c->builder, value, "");
      else
      {
        ast_t* type = deferred_reify(c->frame->reify, ast_type(ast), c->opt);
        bool sign = is_signed(type);
        ast_free_unattached(type);

        neg_result = LLVMBuildNeg(c->builder, value, "");
        if(sign)
          LLVMSetNoSignedWrap(neg_result);
        else
          LLVMSetNoUnsignedWrap(neg_result);
      }

      mutate_unary_variant_t variants[2];
      size_t count = 0;

      if(do_uoi)
      {
        variants[count++] = (mutate_unary_variant_t)
          {NULL, NULL, "identity", NULL};
      }
      if(do_amr && !is_const_fp && !is_const_int)
      {
        if(safe)
        {
          ast_t* type =
            deferred_reify(c->frame->reify, ast_type(ast), c->opt);
          bool sign = is_signed(type);
          ast_free_unattached(type);

          variants[count++] = (mutate_unary_variant_t)
            {make_unsafe_fneg, sign ? make_nsw_neg : make_nuw_neg,
             "neg_unsafe", "amr"};
        }
        else
        {
          variants[count++] = (mutate_unary_variant_t)
            {LLVMBuildFNeg, LLVMBuildNeg, "neg_wrapping", "amr"};
        }
      }

      if(count > 0)
        neg_result = mutate_unary(c, mutate_ast, value, neg_result,
          "neg", "uoi", variants, count);

      neg_result = mutate_cr_unary(c, mutate_ast, ast, value, neg_result,
        safe ? LLVMBuildFNeg : make_unsafe_fneg,
        safe ? LLVMBuildNeg : LLVMBuildNeg);

      goto neg_shadow;
    }
  }

  if(LLVMIsAConstantFP(value))
    neg_result = LLVMBuildFNeg(c->builder, value, "");
  else if(LLVMIsAConstantInt(value))
    neg_result = LLVMConstNeg(value);
  else if(is_fp(value))
    neg_result = (safe ? LLVMBuildFNeg : make_unsafe_fneg)(
      c->builder, value, "");
  else if(safe)
    neg_result = LLVMBuildNeg(c->builder, value, "");
  else
  {
    ast_t* type = deferred_reify(c->frame->reify, ast_type(ast), c->opt);
    bool sign = is_signed(type);
    ast_free_unattached(type);

    neg_result = LLVMBuildNeg(c->builder, value, "");
    if(sign)
      LLVMSetNoSignedWrap(neg_result);
    else
      LLVMSetNoUnsignedWrap(neg_result);
  }

  if(c->opt->mutate && (mutate_ast != NULL))
  {
    neg_result = mutate_cr_unary(c, mutate_ast, ast, value, neg_result,
      safe ? LLVMBuildFNeg : make_unsafe_fneg,
      safe ? LLVMBuildNeg : LLVMBuildNeg);
  }

neg_shadow:
  if(c->opt->mutate)
  {
    LLVMValueRef v_shadow = mutate_shadow_get(c, value);
    if(v_shadow != NULL)
    {
      LLVMValueRef shadow;
      if(is_fp(value))
        shadow = LLVMBuildFNeg(c->builder, v_shadow, "");
      else
        shadow = LLVMConstNeg(v_shadow);
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, neg_result, shadow);
    }
  }

  return neg_result;
}

LLVMValueRef gen_shl(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;
  ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
  bool sign = is_signed(type);
  ast_free_unattached(type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMTypeRef l_type = LLVMTypeOf(l_value);
  pony_assert(LLVMGetTypeKind(l_type) == LLVMIntegerTypeKind);
  unsigned width = LLVMGetIntTypeWidth(l_type);

  LLVMValueRef r_check = c->opt->mutate
    ? mutate_shadow_get(c, r_value) : r_value;
  if((r_check != NULL) && LLVMIsConstant(r_check))
  {
    LLVMTypeRef r_type = LLVMTypeOf(r_check);
    LLVMValueRef width_val = LLVMConstInt(r_type, width, false);
    LLVMValueRef too_wide = LLVMBuildICmp(c->builder, LLVMIntUGE,
      r_check, width_val, "");

    if(is_always_true(too_wide))
    {
      ast_error(c->opt->check.errors, right,
        "shift amount greater than type width");
      return NULL;
    }
  }

  LLVMValueRef result;

  if(c->opt->mutate && (mutate_ast != NULL))
  {
    bool do_sor = mutate_site_enabled(c, "sor", mutate_ast);
    bool do_amr = mutate_site_enabled(c, "amr", mutate_ast);

    if((do_sor || do_amr)
        && ((mutate_shadow_get(c, l_value) == NULL)
            || (mutate_shadow_get(c, r_value) == NULL)))
    {
      result = mutate_shift(c, mutate_ast, l_value, r_value,
        true, sign, safe, width, do_sor, do_amr);

      result = mutate_cr_shift(c, mutate_ast, left, right,
        l_value, r_value, result, true, sign, safe, width);

      goto shl_shadow;
    }
  }

  if(LLVMIsConstant(r_value) && LLVMIsConstant(l_value))
  {
    result = LLVMBuildShl(c->builder, l_value, r_value, "");
  }
  else if(safe)
  {
    LLVMTypeRef r_type = LLVMTypeOf(r_value);
    LLVMValueRef clamp = LLVMConstInt(r_type, width - 1, false);
    LLVMValueRef cmp = LLVMBuildICmp(c->builder, LLVMIntULE, r_value, clamp,
      "");
    LLVMValueRef shift_amount =
      LLVMBuildSelect(c->builder, cmp, r_value, clamp, "");
    result = LLVMBuildShl(c->builder, l_value, shift_amount, "");
  }
  else
  {
    result = LLVMBuildShl(c->builder, l_value, r_value, "");
    if(sign)
      LLVMSetNoSignedWrap(result);
    else
      LLVMSetNoUnsignedWrap(result);
  }

  if(c->opt->mutate && (mutate_ast != NULL))
  {
    result = mutate_cr_shift(c, mutate_ast, left, right,
      l_value, r_value, result, true, sign, safe, width);
  }

shl_shadow:
  if(c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = LLVMBuildShl(c->builder, l_shadow, r_shadow, "");
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }
  }

  return result;
}

LLVMValueRef gen_shr(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;
  ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
  bool sign = is_signed(type);
  ast_free_unattached(type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMTypeRef l_type = LLVMTypeOf(l_value);
  pony_assert(LLVMGetTypeKind(l_type) == LLVMIntegerTypeKind);
  unsigned width = LLVMGetIntTypeWidth(l_type);

  LLVMValueRef r_check = c->opt->mutate
    ? mutate_shadow_get(c, r_value) : r_value;
  if((r_check != NULL) && LLVMIsConstant(r_check))
  {
    LLVMTypeRef r_type = LLVMTypeOf(r_check);
    LLVMValueRef width_val = LLVMConstInt(r_type, width, false);
    LLVMValueRef too_wide = LLVMBuildICmp(c->builder, LLVMIntUGE,
      r_check, width_val, "");

    if(is_always_true(too_wide))
    {
      ast_error(c->opt->check.errors, right,
        "shift amount greater than type width");
      return NULL;
    }
  }

  LLVMValueRef result;

  if(c->opt->mutate && (mutate_ast != NULL))
  {
    bool do_sor = mutate_site_enabled(c, "sor", mutate_ast);
    bool do_amr = mutate_site_enabled(c, "amr", mutate_ast);

    if((do_sor || do_amr)
        && ((mutate_shadow_get(c, l_value) == NULL)
            || (mutate_shadow_get(c, r_value) == NULL)))
    {
      result = mutate_shift(c, mutate_ast, l_value, r_value,
        false, sign, safe, width, do_sor, do_amr);

      result = mutate_cr_shift(c, mutate_ast, left, right,
        l_value, r_value, result, false, sign, safe, width);

      goto shr_shadow;
    }
  }

  if(LLVMIsConstant(r_value) && LLVMIsConstant(l_value))
  {
    if(sign)
      result = LLVMBuildAShr(c->builder, l_value, r_value, "");
    else
      result = LLVMBuildLShr(c->builder, l_value, r_value, "");
  }
  else if(safe)
  {
    LLVMTypeRef r_type = LLVMTypeOf(r_value);
    LLVMValueRef clamp = LLVMConstInt(r_type, width - 1, false);
    LLVMValueRef cmp = LLVMBuildICmp(c->builder, LLVMIntULE, r_value, clamp,
      "");
    LLVMValueRef shift_amount =
      LLVMBuildSelect(c->builder, cmp, r_value, clamp, "");

    if(sign)
      result = LLVMBuildAShr(c->builder, l_value, shift_amount, "");
    else
      result = LLVMBuildLShr(c->builder, l_value, shift_amount, "");
  }
  else
  {
    if(sign)
      result = LLVMBuildAShr(c->builder, l_value, r_value, "");
    else
      result = LLVMBuildLShr(c->builder, l_value, r_value, "");
    LLVMSetIsExact(result);
  }

  if(c->opt->mutate && (mutate_ast != NULL))
  {
    result = mutate_cr_shift(c, mutate_ast, left, right,
      l_value, r_value, result, false, sign, safe, width);
  }

shr_shadow:
  if(c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow;
      if(sign)
        shadow = LLVMBuildAShr(c->builder, l_shadow, r_shadow, "");
      else
        shadow = LLVMBuildLShr(c->builder, l_shadow, r_shadow, "");
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }
  }

  return result;
}

LLVMValueRef gen_and_sc(compile_t* c, ast_t* left, ast_t* right)
{
  return make_short_circuit(c, left, right, true);
}

LLVMValueRef gen_or_sc(compile_t* c, ast_t* left, ast_t* right)
{
  return make_short_circuit(c, left, right, false);
}

LLVMValueRef gen_and(compile_t* c, ast_t* left, ast_t* right)
{
  if(c->opt->mutate && (c->frame->mutate_ast != NULL)
      && mutate_site_enabled(c, "bor", c->frame->mutate_ast))
  {
    mutate_variant_t variants[] = {
      {LLVMBuildOr, LLVMBuildOr, "or", NULL},
      {LLVMBuildXor, LLVMBuildXor, "xor", NULL}
    };
    return mutate_binop(c, c->frame->mutate_ast, left, right,
      LLVMBuildAnd, LLVMBuildAnd, "and", "bor", variants, 2);
  }

  ast_t* mutate_ast = c->frame->mutate_ast;
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMValueRef result = LLVMBuildAnd(c->builder, l_value, r_value, "");

  if(c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);

    if(mutate_ast != NULL)
    {
      result = mutate_cr_binop(c, mutate_ast, left, right,
        l_value, r_value, result, LLVMBuildAnd, LLVMBuildAnd);
    }

    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = LLVMBuildAnd(c->builder, l_shadow, r_shadow, "");
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    if(mutate_ast != NULL)
      result = mutate_uoi_insert(c, mutate_ast, result, l_value, r_value);
  }

  return result;
}

LLVMValueRef gen_or(compile_t* c, ast_t* left, ast_t* right)
{
  if(c->opt->mutate && (c->frame->mutate_ast != NULL)
      && mutate_site_enabled(c, "bor", c->frame->mutate_ast))
  {
    mutate_variant_t variants[] = {
      {LLVMBuildAnd, LLVMBuildAnd, "and", NULL},
      {LLVMBuildXor, LLVMBuildXor, "xor", NULL}
    };
    return mutate_binop(c, c->frame->mutate_ast, left, right,
      LLVMBuildOr, LLVMBuildOr, "or", "bor", variants, 2);
  }

  ast_t* mutate_ast = c->frame->mutate_ast;
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMValueRef result = LLVMBuildOr(c->builder, l_value, r_value, "");

  if(c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);

    if(mutate_ast != NULL)
    {
      result = mutate_cr_binop(c, mutate_ast, left, right,
        l_value, r_value, result, LLVMBuildOr, LLVMBuildOr);
    }

    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = LLVMBuildOr(c->builder, l_shadow, r_shadow, "");
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    if(mutate_ast != NULL)
      result = mutate_uoi_insert(c, mutate_ast, result, l_value, r_value);
  }

  return result;
}

LLVMValueRef gen_xor(compile_t* c, ast_t* left, ast_t* right)
{
  if(c->opt->mutate && (c->frame->mutate_ast != NULL)
      && mutate_site_enabled(c, "bor", c->frame->mutate_ast))
  {
    mutate_variant_t variants[] = {
      {LLVMBuildAnd, LLVMBuildAnd, "and", NULL},
      {LLVMBuildOr, LLVMBuildOr, "or", NULL}
    };
    return mutate_binop(c, c->frame->mutate_ast, left, right,
      LLVMBuildXor, LLVMBuildXor, "xor", "bor", variants, 2);
  }

  ast_t* mutate_ast = c->frame->mutate_ast;
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMValueRef result;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
    result = LLVMConstXor(l_value, r_value);
  else
    result = LLVMBuildXor(c->builder, l_value, r_value, "");

  if(c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);

    if(mutate_ast != NULL)
    {
      result = mutate_cr_binop(c, mutate_ast, left, right,
        l_value, r_value, result, LLVMBuildXor, LLVMBuildXor);
    }

    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = LLVMBuildXor(c->builder, l_shadow, r_shadow, "");
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    if(mutate_ast != NULL)
      result = mutate_uoi_insert(c, mutate_ast, result, l_value, r_value);
  }

  return result;
}

LLVMValueRef gen_not(compile_t* c, ast_t* ast)
{
  ast_t* mutate_ast = c->frame->mutate_ast;
  LLVMValueRef value = gen_expr(c, ast);

  if(value == NULL)
    return NULL;

  ast_t* type = deferred_reify(c->frame->reify, ast_type(ast), c->opt);
  bool bool_type = is_bool(type);
  ast_free_unattached(type);

  LLVMValueRef not_result;

  if(c->opt->mutate && (mutate_ast != NULL)
      && mutate_site_enabled(c, "uoi", mutate_ast))
  {
    if(bool_type)
    {
      if(LLVMIsAConstantInt(value))
        not_result = is_always_true(value)
          ? LLVMConstInt(c->i1, 0, false)
          : LLVMConstInt(c->i1, 1, false);
      else
        not_result = LLVMBuildICmp(c->builder, LLVMIntEQ, value,
          LLVMConstInt(c->i1, 0, false), "");
    }
    else
    {
      if(LLVMIsAConstantInt(value))
        not_result = LLVMConstNot(value);
      else
        not_result = LLVMBuildNot(c->builder, value, "");
    }

    mutate_unary_variant_t variants[] = {
      {NULL, NULL, "identity", NULL}
    };
    not_result = mutate_unary(c, mutate_ast, value, not_result,
      "not", "uoi", variants, 1);

    if(!bool_type)
    {
      not_result = mutate_cr_unary(c, mutate_ast, ast, value, not_result,
        NULL, LLVMBuildNot);
    }

    goto not_shadow;
  }

  if(bool_type)
  {
    if(LLVMIsAConstantInt(value))
    {
      not_result = is_always_true(value)
        ? LLVMConstInt(c->i1, 0, false)
        : LLVMConstInt(c->i1, 1, false);
    }
    else
    {
      not_result = LLVMBuildICmp(c->builder, LLVMIntEQ, value,
        LLVMConstInt(c->i1, 0, false), "");
    }
  }
  else if(LLVMIsAConstantInt(value))
  {
    not_result = LLVMConstNot(value);
  }
  else
  {
    not_result = LLVMBuildNot(c->builder, value, "");
  }

  if(!bool_type && c->opt->mutate && (mutate_ast != NULL))
  {
    not_result = mutate_cr_unary(c, mutate_ast, ast, value, not_result,
      NULL, LLVMBuildNot);
  }

not_shadow:
  if(c->opt->mutate)
  {
    LLVMValueRef v_shadow = mutate_shadow_get(c, value);
    if(v_shadow != NULL)
    {
      LLVMValueRef shadow;
      if(bool_type)
      {
        shadow = is_always_true(v_shadow)
          ? LLVMConstInt(c->i1, 0, false)
          : LLVMConstInt(c->i1, 1, false);
      }
      else
        shadow = LLVMConstNot(v_shadow);
      if(LLVMIsConstant(shadow))
        mutate_shadow_set(c, not_result, shadow);
    }
  }

  return not_result;
}

LLVMValueRef gen_eq(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;

  if(c->opt->mutate && (mutate_ast != NULL)
      && mutate_site_enabled(c, "ror", mutate_ast))
  {
    ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
    bool sign = is_signed(type);
    ast_free_unattached(type);

    LLVMValueRef l_value = gen_expr(c, left);
    LLVMValueRef r_value = gen_expr(c, right);

    if((l_value == NULL) || (r_value == NULL))
      return NULL;

    mutate_cmp_variant_t variants[] = {
      {LLVMRealONE, LLVMIntNE, LLVMIntNE, "ne"},
      {LLVMRealOLT, LLVMIntSLT, LLVMIntULT, "lt"},
      {LLVMRealOLE, LLVMIntSLE, LLVMIntULE, "le"},
      {LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, "ge"},
      {LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, "gt"}
    };
    LLVMValueRef result = mutate_cmp_values(c, mutate_ast, l_value, r_value,
      sign, LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ,
      "eq", "ror", safe, variants, 5);

    result = mutate_cr_cmp(c, mutate_ast, left, right,
      l_value, r_value, result, sign,
      LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, safe);

    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = make_cmp_value(c, sign, l_shadow, r_shadow,
        LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    return result;
  }

  return make_cmp(c, left, right, LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, safe);
}

LLVMValueRef gen_eq_rvalue(compile_t* c, ast_t* left, LLVMValueRef r_value,
  bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;
  ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
  bool sign = is_signed(type);
  ast_free_unattached(type);
  LLVMValueRef l_value = gen_expr(c, left);

  if(c->opt->mutate && (l_value != NULL)
      && (mutate_ast != NULL)
      && mutate_site_enabled(c, "ror", mutate_ast))
  {
    mutate_cmp_variant_t variants[] = {
      {LLVMRealONE, LLVMIntNE, LLVMIntNE, "ne"},
      {LLVMRealOLT, LLVMIntSLT, LLVMIntULT, "lt"},
      {LLVMRealOLE, LLVMIntSLE, LLVMIntULE, "le"},
      {LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, "ge"},
      {LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, "gt"}
    };
    LLVMValueRef result = mutate_cmp_values(c, mutate_ast, l_value, r_value,
      sign, LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ,
      "eq", "ror", safe, variants, 5);

    result = mutate_cr_cmp(c, mutate_ast, left, NULL,
      l_value, r_value, result, sign,
      LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, safe);

    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = make_cmp_value(c, sign, l_shadow, r_shadow,
        LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    return result;
  }

  LLVMValueRef result = make_cmp_value(c, sign, l_value, r_value,
    LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, safe);

  if((result != NULL) && c->opt->mutate && (mutate_ast != NULL))
  {
    result = mutate_cr_cmp(c, mutate_ast, left, NULL,
      l_value, r_value, result, sign,
      LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, safe);
  }

  if((result != NULL) && c->opt->mutate)
  {
    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = make_cmp_value(c, sign, l_shadow, r_shadow,
        LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }
  }

  return result;
}

LLVMValueRef gen_ne(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;

  if(c->opt->mutate && (mutate_ast != NULL)
      && mutate_site_enabled(c, "ror", mutate_ast))
  {
    ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
    bool sign = is_signed(type);
    ast_free_unattached(type);

    LLVMValueRef l_value = gen_expr(c, left);
    LLVMValueRef r_value = gen_expr(c, right);

    if((l_value == NULL) || (r_value == NULL))
      return NULL;

    mutate_cmp_variant_t variants[] = {
      {LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, "eq"},
      {LLVMRealOLT, LLVMIntSLT, LLVMIntULT, "lt"},
      {LLVMRealOLE, LLVMIntSLE, LLVMIntULE, "le"},
      {LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, "ge"},
      {LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, "gt"}
    };
    LLVMValueRef result = mutate_cmp_values(c, mutate_ast, l_value, r_value,
      sign, LLVMRealONE, LLVMIntNE, LLVMIntNE,
      "ne", "ror", safe, variants, 5);

    result = mutate_cr_cmp(c, mutate_ast, left, right,
      l_value, r_value, result, sign,
      LLVMRealONE, LLVMIntNE, LLVMIntNE, safe);

    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = make_cmp_value(c, sign, l_shadow, r_shadow,
        LLVMRealONE, LLVMIntNE, LLVMIntNE, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    return result;
  }

  return make_cmp(c, left, right, LLVMRealONE, LLVMIntNE, LLVMIntNE, safe);
}

LLVMValueRef gen_lt(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;

  if(c->opt->mutate && (mutate_ast != NULL)
      && mutate_site_enabled(c, "ror", mutate_ast))
  {
    ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
    bool sign = is_signed(type);
    ast_free_unattached(type);

    LLVMValueRef l_value = gen_expr(c, left);
    LLVMValueRef r_value = gen_expr(c, right);

    if((l_value == NULL) || (r_value == NULL))
      return NULL;

    mutate_cmp_variant_t variants[] = {
      {LLVMRealOLE, LLVMIntSLE, LLVMIntULE, "le"},
      {LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, "gt"},
      {LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, "ge"},
      {LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, "eq"},
      {LLVMRealONE, LLVMIntNE, LLVMIntNE, "ne"}
    };
    LLVMValueRef result = mutate_cmp_values(c, mutate_ast, l_value, r_value,
      sign, LLVMRealOLT, LLVMIntSLT, LLVMIntULT,
      "lt", "ror", safe, variants, 5);

    result = mutate_cr_cmp(c, mutate_ast, left, right,
      l_value, r_value, result, sign,
      LLVMRealOLT, LLVMIntSLT, LLVMIntULT, safe);

    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = make_cmp_value(c, sign, l_shadow, r_shadow,
        LLVMRealOLT, LLVMIntSLT, LLVMIntULT, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    return result;
  }

  return make_cmp(c, left, right, LLVMRealOLT, LLVMIntSLT, LLVMIntULT, safe);
}

LLVMValueRef gen_le(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;

  if(c->opt->mutate && (mutate_ast != NULL)
      && mutate_site_enabled(c, "ror", mutate_ast))
  {
    ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
    bool sign = is_signed(type);
    ast_free_unattached(type);

    LLVMValueRef l_value = gen_expr(c, left);
    LLVMValueRef r_value = gen_expr(c, right);

    if((l_value == NULL) || (r_value == NULL))
      return NULL;

    mutate_cmp_variant_t variants[] = {
      {LLVMRealOLT, LLVMIntSLT, LLVMIntULT, "lt"},
      {LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, "gt"},
      {LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, "ge"},
      {LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, "eq"},
      {LLVMRealONE, LLVMIntNE, LLVMIntNE, "ne"}
    };
    LLVMValueRef result = mutate_cmp_values(c, mutate_ast, l_value, r_value,
      sign, LLVMRealOLE, LLVMIntSLE, LLVMIntULE,
      "le", "ror", safe, variants, 5);

    result = mutate_cr_cmp(c, mutate_ast, left, right,
      l_value, r_value, result, sign,
      LLVMRealOLE, LLVMIntSLE, LLVMIntULE, safe);

    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = make_cmp_value(c, sign, l_shadow, r_shadow,
        LLVMRealOLE, LLVMIntSLE, LLVMIntULE, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    return result;
  }

  return make_cmp(c, left, right, LLVMRealOLE, LLVMIntSLE, LLVMIntULE, safe);
}

LLVMValueRef gen_ge(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;

  if(c->opt->mutate && (mutate_ast != NULL)
      && mutate_site_enabled(c, "ror", mutate_ast))
  {
    ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
    bool sign = is_signed(type);
    ast_free_unattached(type);

    LLVMValueRef l_value = gen_expr(c, left);
    LLVMValueRef r_value = gen_expr(c, right);

    if((l_value == NULL) || (r_value == NULL))
      return NULL;

    mutate_cmp_variant_t variants[] = {
      {LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, "gt"},
      {LLVMRealOLT, LLVMIntSLT, LLVMIntULT, "lt"},
      {LLVMRealOLE, LLVMIntSLE, LLVMIntULE, "le"},
      {LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, "eq"},
      {LLVMRealONE, LLVMIntNE, LLVMIntNE, "ne"}
    };
    LLVMValueRef result = mutate_cmp_values(c, mutate_ast, l_value, r_value,
      sign, LLVMRealOGE, LLVMIntSGE, LLVMIntUGE,
      "ge", "ror", safe, variants, 5);

    result = mutate_cr_cmp(c, mutate_ast, left, right,
      l_value, r_value, result, sign,
      LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, safe);

    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = make_cmp_value(c, sign, l_shadow, r_shadow,
        LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    return result;
  }

  return make_cmp(c, left, right, LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, safe);
}

LLVMValueRef gen_gt(compile_t* c, ast_t* left, ast_t* right, bool safe)
{
  ast_t* mutate_ast = c->frame->mutate_ast;

  if(c->opt->mutate && (mutate_ast != NULL)
      && mutate_site_enabled(c, "ror", mutate_ast))
  {
    ast_t* type = deferred_reify(c->frame->reify, ast_type(left), c->opt);
    bool sign = is_signed(type);
    ast_free_unattached(type);

    LLVMValueRef l_value = gen_expr(c, left);
    LLVMValueRef r_value = gen_expr(c, right);

    if((l_value == NULL) || (r_value == NULL))
      return NULL;

    mutate_cmp_variant_t variants[] = {
      {LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, "ge"},
      {LLVMRealOLT, LLVMIntSLT, LLVMIntULT, "lt"},
      {LLVMRealOLE, LLVMIntSLE, LLVMIntULE, "le"},
      {LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ, "eq"},
      {LLVMRealONE, LLVMIntNE, LLVMIntNE, "ne"}
    };
    LLVMValueRef result = mutate_cmp_values(c, mutate_ast, l_value, r_value,
      sign, LLVMRealOGT, LLVMIntSGT, LLVMIntUGT,
      "gt", "ror", safe, variants, 5);

    result = mutate_cr_cmp(c, mutate_ast, left, right,
      l_value, r_value, result, sign,
      LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, safe);

    LLVMValueRef l_shadow = mutate_shadow_get(c, l_value);
    LLVMValueRef r_shadow = mutate_shadow_get(c, r_value);
    if((l_shadow != NULL) && (r_shadow != NULL))
    {
      LLVMValueRef shadow = make_cmp_value(c, sign, l_shadow, r_shadow,
        LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, safe);
      if((shadow != NULL) && LLVMIsConstant(shadow))
        mutate_shadow_set(c, result, shadow);
    }

    return result;
  }

  return make_cmp(c, left, right, LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, safe);
}

LLVMValueRef gen_assign(compile_t* c, ast_t* ast)
{
  // Must generate the right hand side before the left hand side.
  AST_GET_CHILDREN(ast, left, right);
  LLVMValueRef r_value = gen_expr(c, right);

  if(r_value == NULL)
    return NULL;

  if(r_value == GEN_NOVALUE)
    return GEN_NOVALUE;

  codegen_debugloc(c, ast);
  ast_t* type = deferred_reify(c->frame->reify, ast_type(right), c->opt);
  LLVMValueRef result = assign_rvalue(c, left, type, r_value);
  ast_free_unattached(type);
  codegen_debugloc(c, NULL);
  return result;
}

LLVMValueRef gen_assign_value(compile_t* c, ast_t* left, LLVMValueRef right,
  ast_t* right_type)
{
  // This is from pattern matching.
  // Must generate the right hand side before the left hand side.
  if(right == NULL)
    return NULL;

  return assign_rvalue(c, left, right_type, right);
}

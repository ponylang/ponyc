#include "genoperator.h"
#include "genexpr.h"
#include "genreference.h"
#include "genname.h"
#include "../pkg/platformfuns.h"
#include "../type/subtype.h"
#include <assert.h>

typedef LLVMValueRef (*const_binop)(LLVMValueRef left, LLVMValueRef right);

typedef LLVMValueRef (*build_binop)(LLVMBuilderRef builder, LLVMValueRef left,
  LLVMValueRef right, const char *name);

static LLVMValueRef assign_rvalue(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value);

static bool is_constant_i1(compile_t* c, LLVMValueRef val)
{
  if(!LLVMIsAConstantInt(val))
    return false;

  LLVMTypeRef type = LLVMTypeOf(val);
  return type == c->i1;
}

static bool is_always_true(compile_t* c, LLVMValueRef val)
{
  if(!is_constant_i1(c, val))
    return false;

  return LLVMConstIntGetZExtValue(val) == 1;
}

static bool is_always_false(compile_t* c, LLVMValueRef val)
{
  if(!is_constant_i1(c, val))
    return false;

  return LLVMConstIntGetZExtValue(val) == 0;
}

static bool is_fp(LLVMValueRef val)
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
  const_binop const_f, const_binop const_i,
  build_binop build_f, build_binop build_i)
{
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
  {
    if(is_fp(l_value))
      return const_f(l_value, r_value);

    return const_i(l_value, r_value);
  }

  if(is_fp(l_value))
  {
    LLVMValueRef result = build_f(c->builder, l_value, r_value, "");

    if(!c->opt->ieee_math)
      LLVMSetUnsafeAlgebra(result);

    return result;
  }

  return build_i(c->builder, l_value, r_value, "");
}

LLVMValueRef make_divmod(compile_t* c, ast_t* left, ast_t* right,
  const_binop const_f, const_binop const_ui, const_binop const_si,
  build_binop build_f, build_binop build_ui, build_binop build_si)
{
  ast_t* type = ast_type(left);
  bool sign = is_signed(c->opt, type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(!is_fp(r_value) &&
    LLVMIsConstant(r_value) &&
    (LLVMConstIntGetSExtValue(r_value) == 0)
    )
  {
    ast_error(right, "constant divide or mod by zero");
    return NULL;
  }

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
  {
    if(is_fp(l_value))
      return const_f(l_value, r_value);

    if(sign)
      return const_si(l_value, r_value);

    return const_ui(l_value, r_value);
  }

  if(is_fp(l_value))
    return build_f(c->builder, l_value, r_value, "");

  // Setup additional blocks.
  LLVMBasicBlockRef insert = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef then_block = codegen_block(c, "div_then");
  LLVMBasicBlockRef post_block = codegen_block(c, "div_post");

  // Check for div by zero.
  LLVMTypeRef r_type = LLVMTypeOf(r_value);
  LLVMValueRef zero = LLVMConstInt(r_type, 0, false);
  LLVMValueRef cmp = LLVMBuildICmp(c->builder, LLVMIntNE, r_value, zero, "");
  LLVMBuildCondBr(c->builder, cmp, then_block, post_block);

  // Divisor is not zero.
  LLVMPositionBuilderAtEnd(c->builder, then_block);
  LLVMValueRef result;

  if(sign)
    result = build_si(c->builder, l_value, r_value, "");
  else
    result = build_ui(c->builder, l_value, r_value, "");

  LLVMBuildBr(c->builder, post_block);

  // Phi node.
  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, r_type, "");
  LLVMAddIncoming(phi, &zero, &insert, 1);
  LLVMAddIncoming(phi, &result, &then_block, 1);

  return phi;
}

static LLVMValueRef make_cmp_value(compile_t* c, bool sign,
  LLVMValueRef l_value, LLVMValueRef r_value, LLVMRealPredicate cmp_f,
  LLVMIntPredicate cmp_si, LLVMIntPredicate cmp_ui)
{
  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
  {
    if(is_fp(l_value))
      return LLVMConstFCmp(cmp_f, l_value, r_value);

    if(sign)
      return LLVMConstICmp(cmp_si, l_value, r_value);

    return LLVMConstICmp(cmp_ui, l_value, r_value);
  }

  if(is_fp(l_value))
    return LLVMBuildFCmp(c->builder, cmp_f, l_value, r_value, "");

  if(sign)
    return LLVMBuildICmp(c->builder, cmp_si, l_value, r_value, "");

  return LLVMBuildICmp(c->builder, cmp_ui, l_value, r_value, "");
}

static LLVMValueRef make_cmp(compile_t* c, ast_t* left, ast_t* right,
  LLVMRealPredicate cmp_f, LLVMIntPredicate cmp_si, LLVMIntPredicate cmp_ui)
{
  ast_t* type = ast_type(left);
  bool sign = is_signed(c->opt, type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  return make_cmp_value(c, sign, l_value, r_value, cmp_f, cmp_si, cmp_ui);
}

LLVMValueRef make_short_circuit(compile_t* c, ast_t* left, ast_t* right,
  bool is_and)
{
  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef next_block = codegen_block(c, "sc_next");
  LLVMBasicBlockRef post_block = codegen_block(c, "sc_post");

  if(is_and)
    LLVMBuildCondBr(c->builder, l_value, next_block, post_block);
  else
    LLVMBuildCondBr(c->builder, l_value, post_block, next_block);

  LLVMPositionBuilderAtEnd(c->builder, next_block);
  LLVMValueRef r_value = gen_expr(c, right);

  if(r_value == NULL)
    return NULL;

  next_block = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, post_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->i1, "");

  LLVMAddIncoming(phi, &l_value, &entry_block, 1);
  LLVMAddIncoming(phi, &r_value, &next_block, 1);

  return phi;
}

static LLVMValueRef assign_one(compile_t* c, LLVMValueRef l_value,
  LLVMValueRef r_value, ast_t* r_type)
{
  LLVMValueRef result = LLVMBuildLoad(c->builder, l_value, "");

  // Cast the rvalue appropriately.
  LLVMTypeRef l_type = LLVMGetElementType(LLVMTypeOf(l_value));
  LLVMValueRef cast_value = gen_assign_cast(c, l_type, r_value, r_type);

  if(cast_value == NULL)
    return NULL;

  // Store to the field.
  LLVMBuildStore(c->builder, cast_value, l_value);
  return result;
}

static bool assign_tuple(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value)
{
  // Handle assignment for each component.
  ast_t* child = ast_child(left);
  ast_t* rtype_child = ast_child(r_type);
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

      case TK_DONTCARE:
        // Ignore this element.
        break;

      default:
        assert(0);
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

  assert(rtype_child == NULL);
  return true;
}

static LLVMValueRef assign_rvalue(compile_t* c, ast_t* left, ast_t* r_type,
  LLVMValueRef r_value)
{
  switch(ast_id(left))
  {
    case TK_VAR:
    case TK_LET:
    {
      // Generate the locals.
      if(gen_localdecl(c, left) == NULL)
        return NULL;

      return assign_rvalue(c, ast_child(left), r_type, r_value);
    }

    case TK_FVARREF:
    case TK_FLETREF:
    {
      // The result is the previous value of the field.
      LLVMValueRef l_value = gen_fieldptr(c, left);
      return assign_one(c, l_value, r_value, r_type);
    }

    case TK_VARREF:
    {
      // The result is the previous value of the local.
      LLVMValueRef l_value = gen_localptr(c, left);
      return assign_one(c, l_value, r_value, r_type);
    }

    case TK_TUPLE:
    {
      // If the l_value is a tuple, assemble it as the result.
      LLVMValueRef result = gen_expr(c, left);

      if(result == NULL)
        return NULL;

      if(!assign_tuple(c, left, r_type, r_value))
        return NULL;

      // Return the original tuple.
      return result;
    }

    case TK_ID:
    {
      // We may have recursed here from a VAR or LET or arrived directly.
      const char* name = ast_name(left);
      LLVMValueRef l_value = codegen_getlocal(c, name);
      return assign_one(c, l_value, r_value, r_type);
    }

    default: {}
  }

  assert(0);
  return NULL;
}

LLVMValueRef gen_add(compile_t* c, ast_t* left, ast_t* right)
{
  return make_binop(c, left, right, LLVMConstFAdd, LLVMConstAdd,
    LLVMBuildFAdd, LLVMBuildAdd);
}

LLVMValueRef gen_sub(compile_t* c, ast_t* left, ast_t* right)
{
  return make_binop(c, left, right, LLVMConstFSub, LLVMConstSub,
    LLVMBuildFSub, LLVMBuildSub);
}

LLVMValueRef gen_mul(compile_t* c, ast_t* left, ast_t* right)
{
  return make_binop(c, left, right, LLVMConstFMul, LLVMConstMul,
    LLVMBuildFMul, LLVMBuildMul);
}

LLVMValueRef gen_div(compile_t* c, ast_t* left, ast_t* right)
{
  return make_divmod(c, left, right, LLVMConstFDiv, LLVMConstUDiv,
    LLVMConstSDiv, LLVMBuildFDiv, LLVMBuildUDiv, LLVMBuildSDiv);
}

LLVMValueRef gen_mod(compile_t* c, ast_t* left, ast_t* right)
{
  return make_divmod(c, left, right, LLVMConstFRem, LLVMConstURem,
    LLVMConstSRem, LLVMBuildFRem, LLVMBuildURem, LLVMBuildSRem);
}

LLVMValueRef gen_neg(compile_t* c, ast_t* ast)
{
  LLVMValueRef value = gen_expr(c, ast);

  if(value == NULL)
    return NULL;

  if(LLVMIsAConstantFP(value))
    return LLVMConstFNeg(value);

  if(LLVMIsAConstantInt(value))
    return LLVMConstNeg(value);

  if(is_fp(value))
    return LLVMBuildFNeg(c->builder, value, "");

  return LLVMBuildNeg(c->builder, value, "");
}

LLVMValueRef gen_shl(compile_t* c, ast_t* left, ast_t* right)
{
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
    return LLVMConstShl(l_value, r_value);

  return LLVMBuildShl(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_shr(compile_t* c, ast_t* left, ast_t* right)
{
  ast_t* type = ast_type(left);
  bool sign = is_signed(c->opt, type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
  {
    if(sign)
      return LLVMConstAShr(l_value, r_value);

    return LLVMConstLShr(l_value, r_value);
  }

  if(sign)
    return LLVMBuildAShr(c->builder, l_value, r_value, "");

  return LLVMBuildLShr(c->builder, l_value, r_value, "");
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
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
    return LLVMConstAnd(l_value, r_value);

  if(is_always_false(c, l_value) || is_always_false(c, r_value))
    return LLVMConstInt(c->i1, 0, false);

  return LLVMBuildAnd(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_or(compile_t* c, ast_t* left, ast_t* right)
{
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
    return LLVMConstOr(l_value, r_value);

  if(is_always_true(c, l_value) || is_always_true(c, r_value))
    return LLVMConstInt(c->i1, 1, false);

  return LLVMBuildOr(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_xor(compile_t* c, ast_t* left, ast_t* right)
{
  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  if(LLVMIsConstant(l_value) && LLVMIsConstant(r_value))
    return LLVMConstXor(l_value, r_value);

  if(is_always_true(c, l_value))
    return LLVMBuildNot(c->builder, r_value, "");

  if(is_always_false(c, l_value))
    return r_value;

  if(is_always_true(c, r_value))
    return LLVMBuildNot(c->builder, l_value, "");

  if(is_always_false(c, r_value))
    return l_value;

  return LLVMBuildXor(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_not(compile_t* c, ast_t* ast)
{
  LLVMValueRef value = gen_expr(c, ast);

  if(value == NULL)
    return NULL;

  if(LLVMIsAConstantInt(value))
    return LLVMConstNot(value);

  return LLVMBuildNot(c->builder, value, "");
}

LLVMValueRef gen_eq(compile_t* c, ast_t* left, ast_t* right)
{
  return make_cmp(c, left, right, LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ);
}

LLVMValueRef gen_eq_rvalue(compile_t* c, ast_t* left, LLVMValueRef r_value)
{
  ast_t* type = ast_type(left);
  bool sign = is_signed(c->opt, type);
  LLVMValueRef l_value = gen_expr(c, left);

  return make_cmp_value(c, sign, l_value, r_value, LLVMRealOEQ, LLVMIntEQ,
    LLVMIntEQ);
}

LLVMValueRef gen_ne(compile_t* c, ast_t* left, ast_t* right)
{
  return make_cmp(c, left, right, LLVMRealONE, LLVMIntNE, LLVMIntNE);
}

LLVMValueRef gen_lt(compile_t* c, ast_t* left, ast_t* right)
{
  return make_cmp(c, left, right, LLVMRealOLT, LLVMIntSLT, LLVMIntULT);
}

LLVMValueRef gen_le(compile_t* c, ast_t* left, ast_t* right)
{
  return make_cmp(c, left, right, LLVMRealOLE, LLVMIntSLE, LLVMIntULE);
}

LLVMValueRef gen_ge(compile_t* c, ast_t* left, ast_t* right)
{
  return make_cmp(c, left, right, LLVMRealOGE, LLVMIntSGE, LLVMIntUGE);
}

LLVMValueRef gen_gt(compile_t* c, ast_t* left, ast_t* right)
{
  return make_cmp(c, left, right, LLVMRealOGT, LLVMIntSGT, LLVMIntUGT);
}

LLVMValueRef gen_assign(compile_t* c, ast_t* ast)
{
  // Must generate the right hand side before the left hand side. Left and
  // right are swapped for type checking, so we fetch them in reverse order.
  AST_GET_CHILDREN(ast, right, left);
  LLVMValueRef r_value = gen_expr(c, right);

  // Emit debug location of assignment expression
  dwarf_location(&c->dwarf, ast);

  if(r_value == NULL)
    return NULL;

  return assign_rvalue(c, left, ast_type(right), r_value);
}

LLVMValueRef gen_assign_value(compile_t* c, ast_t* left, LLVMValueRef right,
  ast_t* right_type)
{
  // Must generate the right hand side before the left hand side.
  if(right == NULL)
    return NULL;

  // This is from pattern matching and we should not generate the alloca again.
  return assign_rvalue(c, ast_child(left), right_type, right);
}

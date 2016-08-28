#include "genoperator.h"
#include "gencall.h"
#include "genexpr.h"
#include "genreference.h"
#include "genname.h"
#include "gentype.h"
#include "../pkg/platformfuns.h"
#include "../type/subtype.h"
#include <assert.h>

typedef LLVMValueRef (*const_binop)(LLVMValueRef left, LLVMValueRef right);

typedef LLVMValueRef (*build_binop)(LLVMBuilderRef builder, LLVMValueRef left,
  LLVMValueRef right, const char *name);

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
  bool sign = is_signed(type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  // This doesn't pick up `x / 0` for 128 bit numbers on platforms without
  // native 128 bit support.
  if(!is_fp(r_value) &&
    LLVMIsConstant(r_value) &&
    (LLVMConstIntGetSExtValue(r_value) == 0)
    )
  {
    ast_error(c->opt->check.errors, right, "constant divide or mod by zero");
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
  bool sign = is_signed(type);

  LLVMValueRef l_value = gen_expr(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  LLVMValueRef test = make_cmp_value(c, sign, l_value, r_value,
    cmp_f, cmp_si, cmp_ui);

  return LLVMBuildZExt(c->builder, test, c->ibool, "");
}

LLVMValueRef make_short_circuit(compile_t* c, ast_t* left, ast_t* right,
  bool is_and)
{
  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef left_block = codegen_block(c, "sc_left");
  LLVMValueRef branch = LLVMBuildBr(c->builder, left_block);

  LLVMPositionBuilderAtEnd(c->builder, left_block);
  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  if(LLVMIsAConstantInt(l_value))
  {
    LLVMInstructionEraseFromParent(branch);
    LLVMDeleteBasicBlock(left_block);
    LLVMPositionBuilderAtEnd(c->builder, entry_block);

    if(is_and)
    {
      if(is_always_false(l_value))
        return gen_expr(c, left);
    } else {
      if(is_always_true(l_value))
        return gen_expr(c, left);
    }

    return gen_expr(c, right);
  }

  LLVMBasicBlockRef left_exit_block = LLVMGetInsertBlock(c->builder);

  LLVMBasicBlockRef right_block = codegen_block(c, "sc_right");
  LLVMBasicBlockRef post_block = codegen_block(c, "sc_post");
  LLVMValueRef test = LLVMBuildTrunc(c->builder, l_value, c->i1, "");

  if(is_and)
    LLVMBuildCondBr(c->builder, test, right_block, post_block);
  else
    LLVMBuildCondBr(c->builder, test, post_block, right_block);

  LLVMPositionBuilderAtEnd(c->builder, right_block);
  LLVMValueRef r_value = gen_expr(c, right);

  if(r_value == NULL)
    return NULL;

  LLVMBasicBlockRef right_exit_block = LLVMGetInsertBlock(c->builder);
  LLVMBuildBr(c->builder, post_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->ibool, "");

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
  LLVMValueRef r_value, ast_t* r_type)
{
  LLVMValueRef result = LLVMBuildLoad(c->builder, l_value, "");

  // Cast the rvalue appropriately.
  LLVMTypeRef cast_type = LLVMGetElementType(LLVMTypeOf(l_value));
  LLVMValueRef cast_value = gen_assign_cast(c, cast_type, r_value, r_type);

  if(cast_value == NULL)
    return NULL;

  // Store to the local.
  LLVMBuildStore(c->builder, cast_value, l_value);

  return result;
}

static LLVMValueRef assign_field(compile_t* c, LLVMValueRef l_value,
  LLVMValueRef r_value, ast_t* p_type, ast_t* r_type)
{
  LLVMValueRef result = LLVMBuildLoad(c->builder, l_value, "");

  // Cast the rvalue appropriately.
  LLVMTypeRef cast_type = LLVMGetElementType(LLVMTypeOf(l_value));
  LLVMValueRef cast_value = gen_assign_cast(c, cast_type, r_value, r_type);

  if(cast_value == NULL)
    return NULL;

  // Store to the field.
  LLVMValueRef store = LLVMBuildStore(c->builder, cast_value, l_value);

  LLVMValueRef metadata = tbaa_metadata_for_type(c, p_type);
  const char id[] = "tbaa";
  LLVMSetMetadata(result, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);
  LLVMSetMetadata(store, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);

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
    case TK_SEQ:
      // The actual expression is inside a sequence node.
      while(ast_id(left) == TK_SEQ)
      {
        assert(ast_childcount(left) == 1);
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

    case TK_FVARREF:
    case TK_FLETREF:
    {
      // The result is the previous value of the field.
      LLVMValueRef l_value = gen_fieldptr(c, left);
      ast_t* p_type = ast_type(ast_child(left));
      return assign_field(c, l_value, r_value, p_type, r_type);
    }

    case TK_EMBEDREF:
    {
      // Do nothing. The embed field was already passed as the receiver.
      return GEN_NOVALUE;
    }

    case TK_VARREF:
    {
      // The result is the previous value of the local.
      const char* name = ast_name(ast_child(left));
      codegen_local_lifetime_start(c, name);
      LLVMValueRef l_value = codegen_getlocal(c, name);
      LLVMValueRef ret = assign_local(c, l_value, r_value, r_type);
      return ret;
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
      codegen_local_lifetime_start(c, name);
      LLVMValueRef l_value = codegen_getlocal(c, name);
      LLVMValueRef ret = assign_local(c, l_value, r_value, r_type);
      return ret;
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
  bool sign = is_signed(type);

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

  return LLVMBuildXor(c->builder, l_value, r_value, "");
}

LLVMValueRef gen_not(compile_t* c, ast_t* ast)
{
  LLVMValueRef value = gen_expr(c, ast);

  if(value == NULL)
    return NULL;

  ast_t* type = ast_type(ast);

  if(is_bool(type))
  {
    if(LLVMIsAConstantInt(value))
    {
      if(is_always_true(value))
        return LLVMConstInt(c->ibool, 0, false);

      return LLVMConstInt(c->ibool, 1, false);
    }

    LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, value,
      LLVMConstInt(c->ibool, 0, false), "");
    return LLVMBuildZExt(c->builder, test, c->ibool, "");
  }

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
  bool sign = is_signed(type);
  LLVMValueRef l_value = gen_expr(c, left);

  LLVMValueRef test = make_cmp_value(c, sign, l_value, r_value,
    LLVMRealOEQ, LLVMIntEQ, LLVMIntEQ);

  return LLVMBuildZExt(c->builder, test, c->ibool, "");
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

  if(r_value == NULL)
    return NULL;

  codegen_debugloc(c, ast);
  LLVMValueRef result = assign_rvalue(c, left, ast_type(right), r_value);
  codegen_debugloc(c, NULL);
  return result;
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

#include "genexpr.h"
#include "genname.h"
#include "genfun.h"
#include "gencontrol.h"
#include "genmatch.h"
#include "genoperator.h"
#include "genreference.h"
#include "gencall.h"
#include "../type/subtype.h"
#include <assert.h>

LLVMValueRef gen_expr(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_SEQ:
      return gen_seq(c, ast);

    case TK_FVARREF:
    case TK_FLETREF:
      return gen_fieldload(c, ast);

    case TK_PARAMREF:
      return gen_param(c, ast);

    case TK_VAR:
    case TK_LET:
      return gen_localdecl(c, ast);

    case TK_VARREF:
    case TK_LETREF:
      return gen_localload(c, ast);

    case TK_IF:
      return gen_if(c, ast);

    case TK_WHILE:
      return gen_while(c, ast);

    case TK_REPEAT:
      return gen_repeat(c, ast);

    case TK_TRY:
      return gen_try(c, ast);

    case TK_MATCH:
      return gen_match(c, ast);

    case TK_CALL:
      return gen_call(c, ast);

    case TK_CONSUME:
    case TK_RECOVER:
      return gen_expr(c, ast_child(ast));

    case TK_BREAK:
      return gen_break(c, ast);

    case TK_CONTINUE:
      return gen_continue(c, ast);

    case TK_RETURN:
      return gen_return(c, ast);

    case TK_ERROR:
      return gen_error(c, ast);

    case TK_NOT:
      return gen_not(c, ast);

    case TK_UNARY_MINUS:
      return gen_neg(c, ast);

    case TK_PLUS:
      return gen_plus(c, ast);

    case TK_MINUS:
      return gen_minus(c, ast);

    case TK_MULTIPLY:
      return gen_multiply(c, ast);

    case TK_DIVIDE:
      return gen_divide(c, ast);

    case TK_MOD:
      return gen_mod(c, ast);

    case TK_LSHIFT:
      return gen_lshift(c, ast);

    case TK_RSHIFT:
      return gen_rshift(c, ast);

    case TK_LT:
      return gen_lt(c, ast);

    case TK_LE:
      return gen_le(c, ast);

    case TK_GE:
      return gen_ge(c, ast);

    case TK_GT:
      return gen_gt(c, ast);

    case TK_EQ:
      return gen_eq(c, ast);

    case TK_NE:
      return gen_ne(c, ast);

    case TK_IS:
      return gen_is(c, ast);

    case TK_ISNT:
      return gen_isnt(c, ast);

    case TK_AND:
      return gen_and(c, ast);

    case TK_OR:
      return gen_or(c, ast);

    case TK_XOR:
      return gen_xor(c, ast);

    case TK_ASSIGN:
      return gen_assign(c, ast);

    case TK_THIS:
      return gen_this(c, ast);

    case TK_INT:
    {
      __int128_t value = ast_int(ast);
      uint64_t low = (uint64_t)value;
      uint64_t high = (uint64_t)(value >> 64);

      LLVMValueRef vlow = LLVMConstInt(c->i128, low, false);
      LLVMValueRef vhigh = LLVMConstInt(c->i128, high, false);
      LLVMValueRef shift = LLVMConstInt(c->i128, 64, false);
      vhigh = LLVMConstShl(vhigh, shift);

      return LLVMConstAdd(vhigh, vlow);
    }

    case TK_FLOAT:
      return LLVMConstReal(c->f64, ast_float(ast));

    case TK_STRING:
      return gen_string(c, ast);

    case TK_TUPLE:
      return gen_tuple(c, ast);

    case TK_AT:
      return gen_ffi(c, ast);

    case TK_COMPILER_INTRINSIC:
      ast_error(ast, "compiler intrinsic not defined");
      return NULL;

    default: {}
  }

  ast_error(ast, "not implemented (codegen unknown)");
  return NULL;
}

static LLVMValueRef box_value(compile_t* c, LLVMValueRef value, ast_t* type)
{
  LLVMTypeRef l_type = LLVMTypeOf(value);

  if(LLVMGetTypeKind(l_type) == LLVMPointerTypeKind)
    return value;

  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  LLVMValueRef box_fn = genfun_box(c, &g);

  if(box_fn != NULL)
    value = codegen_call(c, box_fn, &value, 1);

  return value;
}

static LLVMValueRef unbox_value(compile_t* c, LLVMValueRef value, ast_t* type)
{
  LLVMTypeRef l_type = LLVMTypeOf(value);

  if(LLVMGetTypeKind(l_type) != LLVMPointerTypeKind)
    return value;

  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  LLVMValueRef unbox_fn = genfun_unbox(c, &g);

  if(unbox_fn != NULL)
    value = codegen_call(c, unbox_fn, &value, 1);

  return value;
}

static LLVMValueRef assign_to_tuple(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, ast_t* type)
{
  // Cast each component.
  assert(ast_id(type) == TK_TUPLETYPE);

  int count = LLVMCountStructElementTypes(l_type);
  VLA(LLVMTypeRef, elements, count);
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
      return NULL;

    result = LLVMBuildInsertValue(c->builder, result, cast_value, i, "");
    type_child = ast_sibling(type_child);
    i++;
  }

  return result;
}

LLVMValueRef gen_assign_cast(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, ast_t* type)
{
  if(r_value <= GEN_NOVALUE)
    return r_value;

  LLVMTypeRef r_type = LLVMTypeOf(r_value);

  if(r_type == l_type)
    return r_value;

  switch(LLVMGetTypeKind(l_type))
  {
    case LLVMIntegerTypeKind:
    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
      return unbox_value(c, r_value, type);

    case LLVMPointerTypeKind:
      r_value = box_value(c, r_value, type);
      return LLVMBuildBitCast(c->builder, r_value, l_type, "");

    case LLVMStructTypeKind:
      return assign_to_tuple(c, l_type, r_value, type);

    default: {}
  }

  assert(0);
  return NULL;
}

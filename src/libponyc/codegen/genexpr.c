#include "genexpr.h"
#include "genname.h"
#include "genbox.h"
#include "gencontrol.h"
#include "genident.h"
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

    case TK_IS:
      return gen_is(c, ast);

    case TK_ISNT:
      return gen_isnt(c, ast);

    case TK_ASSIGN:
      return gen_assign(c, ast);

    case TK_THIS:
      return gen_this(c, ast);

    case TK_TRUE:
      return LLVMConstInt(c->i1, 1, false);

    case TK_FALSE:
      return LLVMConstInt(c->i1, 0, false);

    case TK_INT:
      return gen_int(c, ast);

    case TK_FLOAT:
      return gen_float(c, ast);

    case TK_STRING:
      return gen_string(c, ast);

    case TK_TUPLE:
      return gen_tuple(c, ast);

    case TK_FFICALL:
      return gen_ffi(c, ast);

    case TK_AMP:
      return gen_addressof(c, ast);

    case TK_COMPILER_INTRINSIC:
      ast_error(ast, "unimplemented compiler intrinsic");
      LLVMBuildUnreachable(c->builder);
      return GEN_NOVALUE;

    default: {}
  }

  ast_error(ast, "not implemented (codegen unknown)");
  return NULL;
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
      return gen_unbox(c, type, r_value);

    case LLVMPointerTypeKind:
      r_value = gen_box(c, type, r_value);

      if(r_value == NULL)
        return NULL;

      return LLVMBuildBitCast(c->builder, r_value, l_type, "");

    case LLVMStructTypeKind:
      return assign_to_tuple(c, l_type, r_value, type);

    default: {}
  }

  assert(0);
  return NULL;
}

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
  LLVMValueRef ret;
  bool has_scope = ast_has_scope(ast);
  bool has_source = codegen_hassource(c);

  // Dwarf a new lexical scope, if necessary.
  if(has_source && has_scope)
    dwarf_lexicalscope(&c->dwarf, ast);

  switch(ast_id(ast))
  {
    case TK_SEQ:
      ret = gen_seq(c, ast);
      break;

    case TK_FVARREF:
    case TK_FLETREF:
      ret = gen_fieldload(c, ast);
      break;

    case TK_PARAMREF:
      ret = gen_param(c, ast);
      break;

    case TK_VAR:
    case TK_LET:
      ret = gen_localdecl(c, ast);
      break;

    case TK_VARREF:
    case TK_LETREF:
      ret = gen_localload(c, ast);
      break;

    case TK_IF:
      ret = gen_if(c, ast);
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
      ret = gen_expr(c, ast_childidx(ast, 1));
      break;

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

    case TK_AMP:
      ret = gen_addressof(c, ast);
      break;

    case TK_IDENTITY:
      ret = gen_identity(c, ast);
      break;

    case TK_DONTCARE:
      ret = GEN_NOVALUE;
      break;

    case TK_COMPILER_INTRINSIC:
      ast_error(ast, "unimplemented compiler intrinsic");
      LLVMBuildUnreachable(c->builder);
      ret = GEN_NOVALUE;
      break;

    default:
      ast_error(ast, "not implemented (codegen unknown)");
      return NULL;
  }

  if(has_scope && has_source)
    dwarf_finish(&c->dwarf, NULL);

  return ret;
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
      if(LLVMGetTypeKind(r_type) == LLVMPointerTypeKind)
        r_value = gen_unbox(c, type, r_value);

      return assign_to_tuple(c, l_type, r_value, type);

    default: {}
  }

  assert(0);
  return NULL;
}

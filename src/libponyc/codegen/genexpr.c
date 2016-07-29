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
#include "../../libponyrt/mem/pool.h"
#include <assert.h>

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

    case TK_EMBEDREF:
      ret = gen_fieldptr(c, ast);
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
          assert(0);
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
      ret = LLVMConstInt(c->ibool, 1, false);
      break;

    case TK_FALSE:
      ret = LLVMConstInt(c->ibool, 0, false);
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

    case TK_DONTCARE:
      ret = GEN_NOVALUE;
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
  assert(ast_id(type) == TK_TUPLETYPE);

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
    {
      // This can occur if an LLVM intrinsic returns an i1 or a tuple that
      // contains an i1. Extend the i1 to an ibool.
      if((r_type == c->i1) && (l_type == c->ibool))
        return LLVMBuildZExt(c->builder, r_value, l_type, "");

      assert(LLVMGetTypeKind(r_type) == LLVMPointerTypeKind);
      return gen_unbox(c, type, r_value);
    }

    case LLVMPointerTypeKind:
    {
      r_value = gen_box(c, type, r_value);

      if(r_value == NULL)
        return NULL;

      return LLVMBuildBitCast(c->builder, r_value, l_type, "");
    }

    case LLVMStructTypeKind:
    {
      if(LLVMGetTypeKind(r_type) == LLVMPointerTypeKind)
      {
        r_value = gen_unbox(c, type, r_value);
        assert(LLVMGetTypeKind(LLVMTypeOf(r_value)) == LLVMStructTypeKind);
      }

      return assign_to_tuple(c, l_type, r_value, type);
    }

    default: {}
  }

  assert(0);
  return NULL;
}

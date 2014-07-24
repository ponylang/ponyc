#include "gencontrol.h"
#include "../type/subtype.h"
#include <assert.h>

LLVMValueRef gen_expr(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_SEQ:
      return gen_seq(c, ast);

    case TK_ASSIGN:
      return gen_assign(c, ast);

    case TK_THIS:
      return gen_this(c, ast);

    case TK_FVARREF:
    case TK_FLETREF:
      return gen_fieldload(c, ast);

    case TK_PARAMREF:
      return gen_param(c, ast);

    case TK_INT:
      return LLVMConstInt(LLVMIntType(128), ast_int(ast), false);

    default: {}
  }

  ast_error(ast, "not implemented (codegen)");
  return NULL;
}

LLVMValueRef gen_lvalue(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
      ast_error(ast, "not implemented (create local)");
      return NULL;

    case TK_FVARREF:
    case TK_FLETREF:
      return gen_fieldptr(c, ast);

    case TK_VARREF:
    case TK_PARAMREF:
      ast_error(ast, "not implemented (lvalue local)");
      return NULL;

    case TK_TUPLE:
      ast_error(ast, "not implemented (lvalue tuple)");
      return NULL;

    default: {}
  }

  assert(0);
  return NULL;
}

LLVMValueRef gen_seq(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);
  LLVMValueRef value;

  while(child != NULL)
  {
    value = gen_expr(c, child);

    if(value == NULL)
      return NULL;

    child = ast_sibling(child);
  }

  return value;
}

LLVMValueRef gen_assign(compile_t* c, ast_t* ast)
{
  ast_t* left;
  ast_t* right;
  AST_GET_CHILDREN(ast, &left, &right);

  LLVMValueRef l_value = gen_lvalue(c, left);
  LLVMValueRef r_value = gen_expr(c, right);

  if((l_value == NULL) || (r_value == NULL))
    return NULL;

  LLVMValueRef result = LLVMBuildLoad(c->builder, l_value, "");
  LLVMValueRef r_cast = gen_cast(c, left, right, l_value, r_value);

  if(r_cast == NULL)
    return NULL;

  LLVMBuildStore(c->builder, r_cast, l_value);
  return result;
}

LLVMValueRef gen_this(compile_t* c, ast_t* ast)
{
  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMValueRef func = LLVMGetBasicBlockParent(block);
  LLVMValueRef this_ptr = LLVMGetParam(func, 0);

  return this_ptr;
}

LLVMValueRef gen_param(compile_t* c, ast_t* ast)
{
  ast_t* def = ast_get(ast, ast_name(ast_child(ast)));
  size_t index = ast_index(def);

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMValueRef func = LLVMGetBasicBlockParent(block);
  LLVMValueRef param = LLVMGetParam(func, index + 1);

  return param;
}

LLVMValueRef gen_fieldptr(compile_t* c, ast_t* ast)
{
  ast_t* left;
  ast_t* right;
  AST_GET_CHILDREN(ast, &left, &right);

  ast_t* l_type = ast_type(left);
  assert(ast_id(l_type) == TK_NOMINAL);
  assert(ast_id(right) == TK_ID);

  ast_t* def = ast_data(l_type);
  ast_t* field = ast_get(def, ast_name(right));
  size_t index = ast_index(field);
  size_t extra = 1;

  if(ast_id(def) == TK_ACTOR)
    extra++;

  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  LLVMValueRef field_ptr = LLVMBuildStructGEP(c->builder, l_value,
    index + extra, "");

  return field_ptr;
}

LLVMValueRef gen_fieldload(compile_t* c, ast_t* ast)
{
  LLVMValueRef field_ptr = gen_fieldptr(c, ast);

  if(field_ptr == NULL)
    return NULL;

  return LLVMBuildLoad(c->builder, field_ptr, "");
}

LLVMValueRef gen_cast(compile_t* c, ast_t* left, ast_t* right,
  LLVMValueRef l_value, LLVMValueRef r_value)
{
  ast_t* left_type = ast_type(left);
  ast_t* right_type = ast_type(right);

  LLVMTypeRef l_type = LLVMGetElementType(LLVMTypeOf(l_value));
  LLVMTypeRef r_type = LLVMTypeOf(r_value);
  LLVMValueRef r_cast;

  if(is_literal(right_type, "IntLiteral"))
  {
    switch(LLVMGetTypeKind(l_type))
    {
      case LLVMIntegerTypeKind:
      {
        // TODO: determine if left side is signed or unsigned
        (void)left_type;
        r_cast = LLVMConstIntCast(r_value, l_type, true);
        break;
      }

      case LLVMHalfTypeKind:
      case LLVMFloatTypeKind:
      case LLVMDoubleTypeKind:
        // always treat the literal as signed
        r_cast = LLVMConstSIToFP(r_value, l_type);
        break;

      case LLVMPointerTypeKind:
      {
        // TODO: box the integer literal
        ast_error(right_type, "not implemented (boxing int literals)");
        return NULL;
      }

      default:
        assert(0);
        return NULL;
    }
  } else if(is_literal(right_type, "FloatLiteral")) {
    switch(LLVMGetTypeKind(l_type))
    {
      case LLVMHalfTypeKind:
      case LLVMFloatTypeKind:
      case LLVMDoubleTypeKind:
        r_cast = LLVMConstFPCast(r_value, l_type);
        break;

      case LLVMPointerTypeKind:
      {
        // TODO: box the float literal
        ast_error(right_type, "not implemented (boxing float literals)");
        return NULL;
      }

      default:
        assert(0);
        return NULL;
    }
  } else if(LLVMGetTypeKind(l_type) == LLVMPointerTypeKind) {
    switch(LLVMGetTypeKind(r_type))
    {
      case LLVMHalfTypeKind:
      case LLVMFloatTypeKind:
      case LLVMDoubleTypeKind:
      case LLVMIntegerTypeKind:
      {
        // TODO: box the primitive
        ast_error(right_type, "not implemented (boxing primitives)");
        return NULL;
      }

      case LLVMPointerTypeKind:
        r_cast = LLVMBuildBitCast(c->builder, r_value, l_type, "");
        break;

      default:
        assert(0);
        return NULL;
    }
  } else {
    assert(l_type == r_type);
    r_cast = r_value;
  }

  return r_cast;
}

#include "genreference.h"
#include "genexpr.h"
#include <assert.h>

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

LLVMValueRef gen_tuple(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);

  if(ast_sibling(child) == NULL)
    return gen_expr(c, child);

  ast_error(ast, "not implemented (codegen for tuples)");
  return NULL;
}

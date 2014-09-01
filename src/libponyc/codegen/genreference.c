#include "genreference.h"
#include "genexpr.h"
#include "gentype.h"
#include "genname.h"
#include <string.h>
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
  ast_t* def = (ast_t*)ast_get(ast, ast_name(ast_child(ast)));
  size_t index = ast_index(def);

  LLVMBasicBlockRef block = LLVMGetInsertBlock(c->builder);
  LLVMValueRef func = LLVMGetBasicBlockParent(block);
  LLVMValueRef param = LLVMGetParam(func, (unsigned int)(index + 1));

  return param;
}

LLVMValueRef gen_fieldptr(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  ast_t* l_type = ast_type(left);
  assert(ast_id(l_type) == TK_NOMINAL);
  assert(ast_id(right) == TK_ID);

  ast_t* def = (ast_t*)ast_data(l_type);
  ast_t* field = (ast_t*)ast_get(def, ast_name(right));
  size_t index = ast_index(field);
  size_t extra = 1;

  if(ast_id(def) == TK_ACTOR)
    extra++;

  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  LLVMValueRef field_ptr = LLVMBuildStructGEP(c->builder, l_value,
    (unsigned int)(index + extra), "");

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

  // TODO: tuples as rvalues
  ast_error(ast, "not implemented (codegen for tuples)");
  return NULL;
}

LLVMValueRef gen_localdecl(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, idseq, type);

  LLVMTypeRef l_type;
  LLVMValueRef l_value = NULL;
  const char* name;

  ast_t* id = ast_child(idseq);
  ast_t* def;

  if(ast_sibling(id) == NULL)
  {
    l_type = gentype(c, type);

    if(l_type == NULL)
      return NULL;

    name = ast_name(id);
    l_value = LLVMBuildAlloca(c->builder, l_type, name);

    def = (ast_t*)ast_get(ast, name);
    ast_setdata(def, l_value);

    return l_value;
  }

  type = ast_child(type);

  while(id != NULL)
  {
    l_type = gentype(c, type);

    if(l_type == NULL)
      return NULL;

    name = ast_name(id);
    l_value = LLVMBuildAlloca(c->builder, l_type, name);

    def = (ast_t*)ast_get(ast, name);
    ast_setdata(def, l_value);

    id = ast_sibling(id);
    type = ast_sibling(type);
  }

  // TODO: when assigning a tuple to this, we need the individual lvalues
  return l_value;
}

LLVMValueRef gen_localptr(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  const char* name = ast_name(id);
  ast_t* def = (ast_t*)ast_get(ast, name);

  LLVMValueRef value = (LLVMValueRef)ast_data(def);
  assert(value != NULL);

  return value;
}

LLVMValueRef gen_localload(compile_t* c, ast_t* ast)
{
  LLVMValueRef local_ptr = gen_localptr(c, ast);

  if(local_ptr == NULL)
    return NULL;

  return LLVMBuildLoad(c->builder, local_ptr, "");
}

LLVMValueRef gen_string(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  const char* name = ast_name(ast);
  size_t len = strlen(name);

  LLVMValueRef args[4];
  args[0] = LLVMConstInt(LLVMInt32Type(), 0, false);
  args[1] = LLVMConstInt(LLVMInt32Type(), 0, false);

  LLVMValueRef str = LLVMConstString(name, (unsigned int)len, false);
  LLVMValueRef g_str = LLVMAddGlobal(c->module, LLVMTypeOf(str), "$strval");
  LLVMSetInitializer(g_str, str);
  LLVMSetGlobalConstant(g_str, true);
  LLVMValueRef str_ptr = LLVMConstInBoundsGEP(g_str, args, 2);

  LLVMTypeRef l_type = gentype(c, type);
  l_type = LLVMGetElementType(l_type);

  const char* type_name = genname_type(type);
  const char* desc_name = genname_descriptor(type_name);
  LLVMValueRef g_desc = LLVMGetNamedGlobal(c->module, desc_name);

  args[0] = LLVMConstBitCast(g_desc, c->descriptor_ptr);
  args[1] = LLVMConstInt(LLVMInt64Type(), len, false);
  args[2] = LLVMConstInt(LLVMInt64Type(), 0, false);
  args[3] = str_ptr;

  LLVMValueRef inst = LLVMConstNamedStruct(l_type, args, 4);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, l_type, "$string");
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);

  return g_inst;
}

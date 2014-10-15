#include "genreference.h"
#include "genexpr.h"
#include "gentype.h"
#include "genname.h"
#include "../expr/literal.h"
#include <string.h>
#include <assert.h>

LLVMValueRef gen_this(compile_t* c, ast_t* ast)
{
  return LLVMGetParam(codegen_fun(c), 0);
}

LLVMValueRef gen_param(compile_t* c, ast_t* ast)
{
  ast_t* def = ast_get(ast, ast_name(ast_child(ast)), NULL);
  int index = (int)ast_index(def);

  return LLVMGetParam(codegen_fun(c), index + 1);
}

static LLVMValueRef make_fieldptr(compile_t* c, LLVMValueRef l_value,
  ast_t* l_type, ast_t* right)
{
  switch(ast_id(l_type))
  {
    case TK_NOMINAL:
    {
      assert(ast_id(right) == TK_ID);

      ast_t* def = (ast_t*)ast_data(l_type);
      ast_t* field = ast_get(def, ast_name(right), NULL);
      int index = (int)ast_index(field) + 1;

      if(ast_id(def) == TK_ACTOR)
        index++;

      return LLVMBuildStructGEP(c->builder, l_value, index, "");
    }

    case TK_TUPLETYPE:
    {
      assert(ast_id(right) == TK_INT);
      int index = (int)ast_int(right);

      return LLVMBuildExtractValue(c->builder, l_value, index, "");
    }

    case TK_ARROW:
      return make_fieldptr(c, l_value, ast_childidx(l_type, 1), right);

    default: {}
  }

  assert(0);
  return NULL;
}

LLVMValueRef gen_fieldptr(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);

  LLVMValueRef l_value = gen_expr(c, left);

  if(l_value == NULL)
    return NULL;

  ast_t* l_type = ast_type(left);
  return make_fieldptr(c, l_value, l_type, right);
}

LLVMValueRef gen_fieldload(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, left, right);
  ast_t* l_type = ast_type(left);

  LLVMValueRef field = gen_fieldptr(c, ast);

  if(field == NULL)
    return NULL;

  // Don't load if we're reading from a tuple.
  if(ast_id(l_type) != TK_TUPLETYPE)
    field = LLVMBuildLoad(c->builder, field, "");

  return field;
}

LLVMValueRef gen_tuple(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);

  if(ast_sibling(child) == NULL)
    return gen_expr(c, child);

  ast_t* type = ast_type(ast);
  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  LLVMValueRef tuple = LLVMGetUndef(g.primitive);
  int i = 0;

  while(child != NULL)
  {
    LLVMValueRef value = gen_expr(c, child);
    tuple = LLVMBuildInsertValue(c->builder, tuple, value, i++, "");
    child = ast_sibling(child);
  }

  return tuple;
}

LLVMValueRef gen_localdecl(compile_t* c, ast_t* ast)
{
  ast_t* idseq = ast_child(ast);
  ast_t* id = ast_child(idseq);

  while(id != NULL)
  {
    ast_t* type = ast_type(id);

    gentype_t g;

    if(!gentype(c, type, &g))
      return NULL;

    const char* name = ast_name(id);
    LLVMValueRef l_value = LLVMBuildAlloca(c->builder, g.use_type, name);

    ast_t* def = ast_get(ast, name, NULL);
    ast_setdata(def, l_value);

    id = ast_sibling(id);
  }

  return GEN_NOVALUE;
}

LLVMValueRef gen_localptr(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  const char* name = ast_name(id);
  ast_t* def = ast_get(ast, name, NULL);

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

LLVMValueRef gen_int(compile_t* c, ast_t* ast)
{
  // We may not be a concrete literal type here. Pick an appropriate concrete
  // literal type.
  ast_t* type = ast_type(ast);
  type = is_literal_subtype(TK_INTLITERAL, type);

  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  __int128_t value = ast_int(ast);
  uint64_t low = (uint64_t)value;
  uint64_t high = (uint64_t)(value >> 64);

  LLVMValueRef vlow = LLVMConstInt(c->i128, low, false);
  LLVMValueRef vhigh = LLVMConstInt(c->i128, high, false);
  LLVMValueRef shift = LLVMConstInt(c->i128, 64, false);
  vhigh = LLVMConstShl(vhigh, shift);
  vhigh = LLVMConstAdd(vhigh, vlow);

  if(g.primitive == c->i128)
    return vhigh;

  if((g.primitive == c->f32) || (g.primitive == c->f64))
    return LLVMConstUIToFP(vhigh, g.primitive);

  return LLVMConstTrunc(vhigh, g.primitive);
}

LLVMValueRef gen_float(compile_t* c, ast_t* ast)
{
  // We may not be a concrete literal type here. Pick an appropriate concrete
  // literal type.
  ast_t* type = ast_type(ast);
  type = is_literal_subtype(TK_FLOATLITERAL, type);

  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  return LLVMConstReal(g.primitive, ast_float(ast));
}

LLVMValueRef gen_string(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  const char* name = ast_name(ast);
  size_t len = strlen(name);

  LLVMValueRef args[4];
  args[0] = LLVMConstInt(c->i32, 0, false);
  args[1] = LLVMConstInt(c->i32, 0, false);

  LLVMValueRef str = LLVMConstStringInContext(c->context, name, (int)len,
    false);
  LLVMValueRef g_str = LLVMAddGlobal(c->module, LLVMTypeOf(str), "$strval");
  LLVMSetInitializer(g_str, str);
  LLVMSetGlobalConstant(g_str, true);
  LLVMValueRef str_ptr = LLVMConstInBoundsGEP(g_str, args, 2);

  gentype_t g;

  if(!gentype(c, type, &g))
    return NULL;

  args[0] = g.desc;
  args[1] = LLVMConstInt(c->i64, len, false);
  args[2] = LLVMConstInt(c->i64, 0, false);
  args[3] = str_ptr;

  LLVMValueRef inst = LLVMConstNamedStruct(g.structure, args, 4);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, g.structure, "$string");
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);

  return g_inst;
}

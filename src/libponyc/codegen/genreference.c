#include "genreference.h"
#include "genexpr.h"
#include "genname.h"
#include "gencall.h"
#include "gentype.h"
#include "../expr/literal.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include "../type/viewpoint.h"
#include <string.h>
#include <assert.h>

LLVMValueRef gen_this(compile_t* c, ast_t* ast)
{
  (void)ast;
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
      int index = (int)ast_index(field);

      if(ast_id(def) != TK_STRUCT)
        index++;

      if(ast_id(def) == TK_ACTOR)
        index++;

      return LLVMBuildStructGEP(c->builder, l_value, index, "");
    }

    case TK_TUPLETYPE:
    {
      assert(ast_id(right) == TK_INT);
      int index = (int)ast_int(right)->low;

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

  assert((ast_id(l_type) == TK_NOMINAL) || (ast_id(l_type) == TK_TUPLETYPE));

  // Don't load if we're reading from a tuple.
  if(ast_id(l_type) != TK_TUPLETYPE)
  {
    field = LLVMBuildLoad(c->builder, field, "");
    if((ast_id(ast) == TK_FLETREF) || (cap_single(l_type) == TK_VAL))
    {
      LLVMValueRef metadata = LLVMMDNodeInContext(c->context, NULL, 0);
      const char id[] = "invariant.load";
      LLVMSetMetadata(field, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);
    }
    LLVMValueRef metadata = tbaa_metadata_for_type(c, l_type);
    const char id[] = "tbaa";
    LLVMSetMetadata(field, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);
  }

  ast_t* f_type = ast_type(ast);

  if((ast_id(f_type) == TK_NOMINAL) && (cap_single(f_type) == TK_VAL))
  {
    if(LLVMGetTypeKind(LLVMTypeOf(field)) == LLVMPointerTypeKind)
      gencall_invariant_start(c, field);
  }

  return field;
}

LLVMValueRef gen_fieldembed(compile_t* c, ast_t* ast)
{
  LLVMValueRef field = gen_fieldptr(c, ast);

  if(field == NULL)
    return NULL;

  ast_t* f_type = ast_type(ast);

  if((ast_id(f_type) == TK_NOMINAL) && (cap_single(f_type) == TK_VAL))
  {
    if(LLVMGetTypeKind(LLVMTypeOf(field)) == LLVMPointerTypeKind)
      gencall_invariant_start(c, field);
  }

  return field;
}

LLVMValueRef gen_tuple(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);

  if(ast_sibling(child) == NULL)
    return gen_expr(c, child);

  ast_t* type = ast_type(ast);

  // If we contain TK_DONTCARE, we have no usable value.
  if(contains_dontcare(type))
    return GEN_NOTNEEDED;

  reach_type_t* t = reach_type(c->reach, type);
  LLVMValueRef tuple = LLVMGetUndef(t->primitive);
  int i = 0;

  while(child != NULL)
  {
    LLVMValueRef value = gen_expr(c, child);

    if(value == NULL)
      return NULL;

    // We'll have an undefined element if one of our source elements is a
    // variable declaration. This is ok, since the tuple value will never be
    // used.
    if(value == GEN_NOVALUE)
      return GEN_NOTNEEDED;

    tuple = LLVMBuildInsertValue(c->builder, tuple, value, i++, "");
    child = ast_sibling(child);
  }

  return tuple;
}

LLVMValueRef gen_localdecl(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  ast_t* type = ast_type(id);
  const char* name = ast_name(id);

  // If this local has already been generated, don't create another copy. This
  // can happen when the same ast node is generated more than once, such as
  // the condition block of a while expression.
  LLVMValueRef value = codegen_getlocal(c, name);

  if(value != NULL)
    return GEN_NOVALUE;

  reach_type_t* t = reach_type(c->reach, type);

  // All alloca should happen in the entry block of a function.
  LLVMBasicBlockRef this_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(codegen_fun(c));
  LLVMValueRef inst = LLVMGetFirstInstruction(entry_block);

  if(inst != NULL)
    LLVMPositionBuilderBefore(c->builder, inst);
  else
    LLVMPositionBuilderAtEnd(c->builder, entry_block);

  LLVMValueRef alloc = LLVMBuildAlloca(c->builder, t->use_type, name);

  // Store the alloca to use when we reference this local.
  codegen_setlocal(c, name, alloc);

  LLVMMetadataRef file = codegen_difile(c);
  LLVMMetadataRef scope = codegen_discope(c);

  LLVMMetadataRef info = LLVMDIBuilderCreateAutoVariable(c->di, scope, name,
    file, (unsigned)ast_line(ast), t->di_type);

  LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(c->di, NULL, 0);

  LLVMDIBuilderInsertDeclare(c->di, alloc, info, expr,
    (unsigned)ast_line(ast), (unsigned)ast_pos(ast), scope,
    LLVMGetInsertBlock(c->builder));

  // Put the builder back where it was.
  LLVMPositionBuilderAtEnd(c->builder, this_block);
  return GEN_NOVALUE;
}

LLVMValueRef gen_localptr(compile_t* c, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  const char* name = ast_name(id);

  LLVMValueRef value = codegen_getlocal(c, name);
  assert(value != NULL);

  return value;
}

LLVMValueRef gen_localload(compile_t* c, ast_t* ast)
{
  LLVMValueRef local_ptr = gen_localptr(c, ast);

  if(local_ptr == NULL)
    return NULL;

  LLVMValueRef local = LLVMBuildLoad(c->builder, local_ptr, "");

  if(ast_id(ast) == TK_LETREF)
  {
    LLVMValueRef metadata = LLVMMDNodeInContext(c->context, NULL, 0);
    const char id[] = "invariant.load";
    LLVMSetMetadata(local, LLVMGetMDKindID(id, sizeof(id) - 1), metadata);
  }

  ast_t* l_type = ast_type(ast);

  if((ast_id(l_type) == TK_NOMINAL) && (cap_single(l_type) == TK_VAL))
  {
    if(LLVMGetTypeKind(LLVMTypeOf(local)) == LLVMPointerTypeKind)
      gencall_invariant_start(c, local);
  }

  return local;
}

LLVMValueRef gen_addressof(compile_t* c, ast_t* ast)
{
  ast_t* expr = ast_child(ast);

  switch(ast_id(expr))
  {
    case TK_VARREF:
      return gen_localptr(c, expr);

    case TK_FVARREF:
      return gen_fieldptr(c, expr);

    case TK_FUNREF:
    case TK_BEREF:
      return gen_funptr(c, expr);

    default: {}
  }

  assert(0);
  return NULL;
}

static LLVMValueRef gen_digestof_value(compile_t* c, LLVMValueRef value)
{
  LLVMTypeRef type = LLVMTypeOf(value);

  switch(LLVMGetTypeKind(type))
  {
    case LLVMFloatTypeKind:
      value = LLVMBuildBitCast(c->builder, value, c->i32, "");
      return LLVMBuildZExt(c->builder, value, c->i64, "");

    case LLVMDoubleTypeKind:
      return LLVMBuildBitCast(c->builder, value, c->i64, "");

    case LLVMIntegerTypeKind:
    {
      uint32_t width = LLVMGetIntTypeWidth(type);

      if(width < 64)
      {
        value = LLVMBuildZExt(c->builder, value, c->i64, "");
      } else if(width == 128) {
        LLVMValueRef shift = LLVMConstInt(c->i128, 64, false);
        LLVMValueRef high = LLVMBuildLShr(c->builder, value, shift, "");
        high = LLVMBuildTrunc(c->builder, high, c->i64, "");
        value = LLVMBuildTrunc(c->builder, value, c->i64, "");
        value = LLVMBuildXor(c->builder, value, high, "");
      }

      return value;
    }

    case LLVMStructTypeKind:
    {
      uint32_t count = LLVMCountStructElementTypes(type);
      LLVMValueRef result = LLVMConstInt(c->i64, 0, false);

      for(uint32_t i = 0; i < count; i++)
      {
        LLVMValueRef elem = LLVMBuildExtractValue(c->builder, value, i, "");
        elem = gen_digestof_value(c, elem);
        result = LLVMBuildXor(c->builder, result, elem, "");
      }

      return result;
    }

    case LLVMPointerTypeKind:
      return LLVMBuildPtrToInt(c->builder, value, c->i64, "");

    default: {}
  }

  assert(0);
  return NULL;
}

LLVMValueRef gen_digestof(compile_t* c, ast_t* ast)
{
  ast_t* expr = ast_child(ast);
  LLVMValueRef value = gen_expr(c, expr);
  return gen_digestof_value(c, value);
}

LLVMValueRef gen_int(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  reach_type_t* t = reach_type(c->reach, type);

  lexint_t* value = ast_int(ast);
  LLVMValueRef vlow = LLVMConstInt(c->i128, value->low, false);
  LLVMValueRef vhigh = LLVMConstInt(c->i128, value->high, false);
  LLVMValueRef shift = LLVMConstInt(c->i128, 64, false);
  vhigh = LLVMConstShl(vhigh, shift);
  vhigh = LLVMConstAdd(vhigh, vlow);

  if(t->primitive == c->i128)
    return vhigh;

  if((t->primitive == c->f32) || (t->primitive == c->f64))
    return LLVMConstUIToFP(vhigh, t->primitive);

  return LLVMConstTrunc(vhigh, t->primitive);
}

LLVMValueRef gen_float(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  reach_type_t* t = reach_type(c->reach, type);

  return LLVMConstReal(t->primitive, ast_float(ast));
}

LLVMValueRef gen_string(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  const char* name = ast_name(ast);
  size_t len = ast_name_len(ast);

  LLVMValueRef args[4];
  args[0] = LLVMConstInt(c->i32, 0, false);
  args[1] = LLVMConstInt(c->i32, 0, false);

  LLVMValueRef str = LLVMConstStringInContext(c->context, name, (int)len,
    false);
  LLVMValueRef g_str = LLVMAddGlobal(c->module, LLVMTypeOf(str), "");
  LLVMSetLinkage(g_str, LLVMPrivateLinkage);
  LLVMSetInitializer(g_str, str);
  LLVMSetGlobalConstant(g_str, true);
  LLVMValueRef str_ptr = LLVMConstInBoundsGEP(g_str, args, 2);

  reach_type_t* t = reach_type(c->reach, type);

  args[0] = t->desc;
  args[1] = LLVMConstInt(c->intptr, len, false);
  args[2] = LLVMConstInt(c->intptr, len + 1, false);
  args[3] = str_ptr;

  LLVMValueRef inst = LLVMConstNamedStruct(t->structure, args, 4);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, t->structure, "");
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);
  LLVMSetLinkage(g_inst, LLVMPrivateLinkage);

  return g_inst;
}
